// #include "../lib/libpng/png.h"
// #include <png.h>
#include "lpng1639/png.h"

static png_voidp genmalloc_fn(png_structp png_ptr, png_alloc_size_t size)
{
    png_voidp ptr = zidmalloc(TEMPORARYZONE, size);
    return ptr;
}

static void genfree_fn(png_structp png_ptr, png_voidp ptr)
{
    zidfree(ptr);
}

int readPNG(const char *loadfile, textureImage_t *texImg ) {
    FILE *fp;
    unsigned char header[8];
    int readnum;
    unsigned int width, height;
    png_bytep data;
    png_bytep rowptr[65535];
    
    readnum = 8;
    fp = fopen(loadfile, "r");
    if(!fp) {
        printf("%s file not found \n", loadfile);
    }

    fread(header, 1, readnum, fp);
    if(png_sig_cmp(header, 0, readnum)) {
        fclose(fp);
        return 1;
    }

    png_voidp error_ptr = NULL;
    png_error_ptr errfn = NULL;
    png_error_ptr warnfn = NULL;
    png_voidp mem_ptr = NULL;

    png_structp png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
    error_ptr,
    errfn,
    warnfn,
    mem_ptr,
    &genmalloc_fn,
    &genfree_fn);

    if(!png_ptr) {
        printf("png ptr fail \n");
        return 1;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) {
        printf("png infoptr fail \n");
        png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)NULL);
        return 1;
    }

    if(setjmp(png_jmpbuf(png_ptr))) {
        printf("jump buf error\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        fclose(fp);
        return 1;
    }
    
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, readnum);
    png_read_info(png_ptr, info_ptr);

    int bit_depth, color_type, interlace_method, compression_method, filter_method;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
    &color_type, &interlace_method, &compression_method, &filter_method);

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
        png_set_add_alpha(png_ptr,255, PNG_FILLER_AFTER);
        png_read_update_info(png_ptr, info_ptr);
    }

    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    data = (png_bytep)zidmalloc(PERMANENTZONE, rowbytes*height);
    for(unsigned int i = 0; i < height; i++) {
        rowptr[i] = (data + rowbytes*i);
    }

    png_read_image(png_ptr, rowptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose(fp);

    texImg->width = width;
    texImg->height = height;
    texImg->data = data;


    return 0;
}
