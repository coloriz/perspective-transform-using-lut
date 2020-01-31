#include "common.hpp"


namespace ins
{


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


LUT::LUT(Mat transform_matrix, int width, int height, uint* datastart)
{
    table_size = width * height;
    lookup_table = make_unique<uint*[]>(table_size);

    vector<Point2f> coordinates_map;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            coordinates_map.push_back(Point2f(x, y));
        }
    }

    perspectiveTransform(coordinates_map, coordinates_map, transform_matrix);

    for (int i = 0; i < table_size; i++)
    {
        Point2i point = Point2i(static_cast<int>(roundf(coordinates_map[i].x)), static_cast<int>(roundf(coordinates_map[i].y)));
        int offset = point.y * width + point.x;
        lookup_table[i] = datastart + offset;
    }
}


PlainLUT::PlainLUT(Mat transform_matrix, int width, int height, uint* datastart)
    : LUT(transform_matrix, width, height, datastart)
{
}

void PlainLUT::apply(const uint* image_data)
{
    uint** lut = lookup_table.get();

    for (int i = 0; i < table_size; i++)
    {
        **lut++ = *image_data++;
    }
}


ParallelLUT::ParallelLUT(Mat transform_matrix, int width, int height, uint* datastart)
    : LUT(transform_matrix, width, height, datastart)
{
    n_threads = getNumThreads();
}

void ParallelLUT::apply(const uint* image_data)
{
    uint** lut = lookup_table.get();

    parallel_for_(Range(0, table_size), [&](const Range& range){
        uint** lut_partial = lut + range.start;
        const uint* image_partial = image_data + range.start;
        for (int r = range.start; r < range.end; r++)
        {
            **lut_partial++ = *image_partial++;
        }
    }, n_threads);
}


#ifdef __arm__
LoadStoreMultipleLUT::LoadStoreMultipleLUT(Mat transform_matrix, int width, int height, uint* datastart)
    : LUT(transform_matrix, width, height, datastart)
{
}

void LoadStoreMultipleLUT::apply(const uint* image_data)
{
    uint** lut = lookup_table.get();

    __asm__ volatile (
        "mov     r0, #0\n"
        ".lsmloop:\n\t"
        "cmp     r0, %[table_size]\n\t"
        "bge     .lsmexit\n\t"
        "ldmia   %[image_data]!, {r1-r4}\n\t"
        "ldmia   %[lut]!, {r5-r8}\n\t"
        "str     r1, [r5]\n\t"
        "str     r2, [r6]\n\t"
        "str     r3, [r7]\n\t"
        "str     r4, [r8]\n\t"
        "add     r0, #4\n\t"
        "b       .lsmloop\n"
        ".lsmexit:"
        :
        : [lut]"r" (lut), [image_data]"r" (image_data), [table_size]"r" (table_size)
        : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8"
    );
}


ParallelLoadStoreMultipleLUT::ParallelLoadStoreMultipleLUT(Mat transform_matrix, int width, int height, uint* datastart)
    : LUT(transform_matrix, width, height, datastart)
{
    n_threads = getNumThreads();
}

void ParallelLoadStoreMultipleLUT::apply(const uint* image_data)
{
    uint** lut = lookup_table.get();

    parallel_for_(Range(0, table_size), [&](const Range& range){
        __asm__ volatile (
            "add     %[lut], %[lut], %[start], LSL #2\n\t"
            "add     %[image_data], %[image_data], %[start], LSL #2\n\t"
            "mov     r0, %[start]\n"
            ".plsmloop:\n\t"
            "cmp     r0, %[end]\n\t"
            "bge     .plsmexit\n\t"
            "ldmia   %[image_data]!, {r1-r4}\n\t"
            "ldmia   %[lut]!, {r5-r8}\n\t"
            "str     r1, [r5]\n\t"
            "str     r2, [r6]\n\t"
            "str     r3, [r7]\n\t"
            "str     r4, [r8]\n\t"
            "add     r0, #4\n\t"
            "b       .plsmloop\n"
            ".plsmexit:"
            :
            : [lut]"r" (lut), [image_data]"r" (image_data), [start]"r" (range.start), [end]"r" (range.end)
            : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8"
        );
    }, n_threads);
}
#endif


}