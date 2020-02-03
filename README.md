# Perspective Transform using LUT Speed Test

## 실험 환경
- Raspberry Pi 3 B+ & macOS Catalina
- C++ 17
- OpenCV >= 4.1

## 실험 방법
- 영상은 1920x1080 8bpp 4채널 RGBA라고 가정
- 각 알고리즘의 경과 시간은 100번 수행의 평균

## 변수 설명
```C++
constexpr auto DISPLAY_W = 1920;
constexpr auto DISPLAY_H = 1080;
int table_size = DISPLAY_W * DISPLAY_H;
uint32_t* frame;    // DISPLAY_W x DISPLAY_H x 4 크기, 콘텐츠의 한 프레임
int n_threads = cv::getNumThreads();    // 쓰레드 개수

// 원본 프레임의 인덱스 -> screen 버퍼의 해당 픽셀 주소 매핑 LUT
auto lookup_table = make_unique<uint32_t*[]>(table_size);
```

## LUT 생성 과정

### 1. 변환 행렬(Homography matrix) 생성
4개의 점 `(0, 0), (DISPLAY_W - 1, 0), (DISPLAY_W - 1, DISPLAY_H - 1), (0, DISPLAY_H - 1)`이 각각 어디로 변환되어야 하는가를 기술하는 행렬
```C++
Mat get_transform_matrix(vector<Point2f> desired_points)
{
    vector<Point2f> image_rect = {
        Point2f(0, 0),
        Point2f(DISPLAY_W - 1, 0),
        Point2f(DISPLAY_W - 1, DISPLAY_H - 1),
        Point2f(0, DISPLAY_H - 1)
    };

    return getPerspectiveTransform(image_rect, desired_points);
}
```

### 2. LUT 생성
`[frame index]` -> `[screen의 주소]` 매핑 관계 생성
```C++
/* (0, 0), (1, 0), (2, 0) ... (DISPLAY_W, DISPLAY_H) 까지
 * 화면의 모든 점이 변환 행렬에 의해 어떤 지점으로 바뀌는지 계산
 */
vector<Point2f> coordinates_map;

for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
        coordinates_map.push_back(Point2f(x, y));

perspectiveTransform(coordinates_map, coordinates_map, transform_matrix);

/* 위에서 구한 변환된 좌표들을 1차원으로 만들고 최종 LUT 생성
 * display_buffer는 실제 디스플레이 장치의 주소값
 */
for (int i = 0; i < table_size; i++)
{
    Point2i point = Point2i(static_cast<int>(roundf(coordinates_map[i].x)), static_cast<int>(roundf(coordinates_map[i].y)));
    int offset = point.y * width + point.x;
    lookup_table[i] = display_buffer + offset;
}
```

## Experiments

### Plain LUT (simple for-loop)
```C++
uint32_t** lut = lookup_table.get();

for (int i = 0; i < table_size; i++)
    **lut++ = *frame++;
```
💡위 코드에서 `for` 루프 부분 어셈블리 컴파일 결과 (ARM Raspbian Buster GCC 8.3.0)↓
```ASM
        mov     r3, #0
        str     r3, [fp, #-12]
.L17:
        ldr     r3, [fp, #-16]
        ldr     r3, [r3, #8]
        ldr     r2, [fp, #-12]
        cmp     r2, r3
        bge     .L18
        ldr     r3, [fp, #-20]
        add     r2, r3, #4
        str     r2, [fp, #-20]
        ldr     r2, [fp, #-8]
        add     r1, r2, #4
        str     r1, [fp, #-8]
        ldr     r2, [r2]
        ldr     r3, [r3]
        str     r3, [r2]
        ldr     r3, [fp, #-12]
        add     r3, r3, #1
        str     r3, [fp, #-12]
        b       .L17
.L18:
```

### Multi-threaded LUT access
- 멀티 쓰레드 라이브러리 : pthread
- OpenCV의 `cv::parallel_for_` 함수 사용
```C++
uint32_t** lut = lookup_table.get();

cv::parallel_for_(cv::Range(0, table_size), [&](const cv::Range& range){
    uint** lut_partial = lut + range.start;
    const uint* frame_partial = frame + range.start;
    for (int r = range.start; r < range.end; r++)
    {
        **lut_partial++ = *frame_partial++;
    }
}, n_threads);
```

### ARM 명령어 LDM을 사용하여 메모리 접근 최적화
프레임 데이터, lut에서 데이터를 load할 때 general purpose 레지스터 8개(`r1-r8`)를 이용해 각 4개씩 한 번에 16 Bytes를 가져옴
```C++
uint32_t** lut = lookup_table.get();

__asm__ volatile (
    "mov     r0, %[table_size]\n"
    ".loop:\n\t"
    "ldmia   %[frame]!, {r1-r4}\n\t"
    "ldmia   %[lut]!, {r5-r8}\n\t"
    "str     r1, [r5]\n\t"
    "str     r2, [r6]\n\t"
    "str     r3, [r7]\n\t"
    "str     r4, [r8]\n\t"
    "subs    r0, #4\n\t"
    "bgt     .loop\n\t"
    :
    : [lut] "r" (lut), [frame] "r" (frame), [table_size] "r" (table_size)
    : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8"
);
```

### 멀티 쓰레드 + LDM 최적화
```C++
uint32_t** lut = lookup_table.get();

parallel_for_(Range(0, table_size), [&](const Range& range){
    __asm__ volatile (
        "add     %[lut], %[lut], %[start], LSL #2\n\t"
        "add     %[frame], %[frame], %[start], LSL #2\n\t"
        "mov     r0, %[start]\n"
        ".plsmloop:\n\t"
        "cmp     r0, %[end]\n\t"
        "bge     .plsmexit\n\t"
        "ldmia   %[frame]!, {r1-r4}\n\t"
        "ldmia   %[lut]!, {r5-r8}\n\t"
        "str     r1, [r5]\n\t"
        "str     r2, [r6]\n\t"
        "str     r3, [r7]\n\t"
        "str     r4, [r8]\n\t"
        "add     r0, #4\n\t"
        "b       .plsmloop\n"
        ".plsmexit:"
        :
        : [lut]"r" (lut), [frame]"r" (frame), [start]"r" (range.start), [end]"r" (range.end)
        : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8"
    );
}, n_threads);
```

## 실험 결과
> 全ての実験はRPi3b+で実行され、各々の実行時間はそのメソードで100回実行した値の平均である。

|        METHOD        | EXECUTION TIME |
|:--------------------:|:--------------:|
|       Plain LUT      |      42ms      |
|  Multi-threaded LUT  |      17ms      |
|          LDM         |      17ms      |
| Multi-threaded + LDM |      17ms      |

## Appendix
### 라즈베리파이 모델별 상세 스펙
- Raspberry Pi 3 Model B+ (<https://www.raspberrypi.org/products/raspberry-pi-3-model-b-plus/>)
    - Broadcom BCM2837B0, Quad core Cortex-A53 (ARMv8) 64-bit SoC @ 1.4Ghz
    - 1GB LPDDR2 SDRAM
    - 1 x full size HDMI
    - H.264, MPEG-4 decode (1080p30)
    - H264 encode (1080p30)
    - OpenGL ES 1.1, 2.0 Graphics
- Raspberry Pi 4 Model B (<https://www.raspberrypi.org/products/raspberry-pi-4-model-b/specifications/>)
    - Broadcom BCM2711, Quad core Cortex-A72 (ARMv8) 64-bit SoC @ 1.5GHz
    - 4GB LPDDR4-3200 SDRAM
    - 2 x micro-HDMI ports (up to 4kp60 supported)
    - H.265 (4kp60 decode), H264 (1080p60 decode, 1080p30 encode)
    - OpenGL ES 3.0 Graphics