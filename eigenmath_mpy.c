
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include <errno.h>
#include "py/obj.h"
//#include "py/mpconfig.h"
#include "py/misc.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/binary.h"
#include "py/gc.h"
#include "py/stream.h"
#include "eigenmath.h"
#include "eheap.h"
//-DPICO_STACK_SIZE=0x4000 ??

typedef struct _mp_obj_eigenmath_t {
    mp_obj_base_t base;
    size_t heapSize;
    uint8_t  *pHeap;
} mp_obj_eigenmath_t;

static void eigenmath_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_printf(print, "<EigenMath instance>");
}

static mp_obj_t eigenmath_make_new(const mp_obj_type_t *type,
	size_t n_args, size_t n_kw,
	const mp_obj_t *args) {

	mp_arg_check_num(n_args, n_kw, 1, 1, false);
	mp_obj_eigenmath_t *self = mp_obj_malloc(mp_obj_eigenmath_t, type);
	self->base.type = type;
    self->heapSize = mp_obj_get_int(args[0]); // 350 * 1024; // 350KB
    //mp_printf(&mp_plat_print,"heapSize = %d\n", self->heapSize);

    self->pHeap = (uint8_t *)m_malloc(self->heapSize);

    //mp_printf(&mp_plat_print,"ptemp = %x\n", (uint32_t)(ptemp));
    //mp_printf(&mp_plat_print,"self->pHeap = %x\n", (uint32_t)(self->pHeap));
	if (self->pHeap == NULL){
		mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to initialize heap"));
		return MP_OBJ_NULL;
	} 
    

    eigenmath_init(self->pHeap,self->heapSize);

	return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t eigenmath_run(mp_obj_t self_in, mp_obj_t input_str_obj) {
	//mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	size_t len;
    noprint = false;
    const char *buf = mp_obj_str_get_data(input_str_obj, &len);

	//GET_STR_DATA_LEN(input_str_obj, str, str_len);
	run((char *)buf);


	return mp_const_none;

}
static MP_DEFINE_CONST_FUN_OBJ_2(eigenmath_run_obj, eigenmath_run);

static mp_obj_t eigenmath_calc(mp_obj_t self_in, mp_obj_t input_str_obj) {
	//mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	size_t len;
    noprint = true;
    const char *buf = mp_obj_str_get_data(input_str_obj, &len);

	run((char *)buf);

    //size_t len = strlen(outbuf);

    mp_obj_t bytearray = mp_obj_new_bytearray_by_ref(outbuf_length, outbuf);

    // return memoryview
    return mp_obj_new_memoryview(BYTEARRAY_TYPECODE, bytearray);

}
static MP_DEFINE_CONST_FUN_OBJ_2(eigenmath_calc_obj, eigenmath_calc);



static mp_obj_t eigenmath_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    //mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);

    if (n_args != 1 || n_kw != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("Expected 1 positional argument"));
    }

    const char *cmd = mp_obj_str_get_str(args[0]);
    run((char *)cmd);  // 
    return mp_const_none;
}



static mp_obj_t eigenmath_runfile(mp_obj_t self_in, mp_obj_t input_file_obj) {
    const mp_stream_p_t *stream_p = mp_get_stream_raise(input_file_obj, MP_STREAM_OP_READ | MP_STREAM_OP_IOCTL);

    int error = 0;

    // get file size
    struct mp_stream_seek_t seek = {
        .offset = 0,
        .whence = MP_SEEK_END,
    };
    if (stream_p->ioctl(input_file_obj, MP_STREAM_SEEK, (uintptr_t)&seek, &error) == MP_STREAM_ERROR) {
        mp_raise_OSError(error);
    }
    mp_off_t size = seek.offset;

    // move to front
    seek.offset = 0;
    seek.whence = MP_SEEK_SET;
    if (stream_p->ioctl(input_file_obj, MP_STREAM_SEEK, (uintptr_t)&seek, &error) == MP_STREAM_ERROR) {
        mp_raise_OSError(error);
    }

    // get buffer
    char *buf = m_new(char, size + 1);

    // read file
    mp_uint_t out_sz = stream_p->read(input_file_obj, buf, size, &error);
    if (error != 0 || out_sz != size) {
        m_del(char, buf, size + 1);
        mp_raise_OSError(error);
    }

    // add end
    buf[out_sz] = '\0';

    // run
    run(buf);

    // release buffer
    m_del(char, buf, size + 1);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(eigenmath_runfile_obj, eigenmath_runfile);





extern int free_count;
extern int MAXATOMS;
static mp_obj_t eigenmath_status(mp_obj_t self_in) {
	//mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	int fragmentation = e_heap_fragmentation();
    size_t free_bytes = e_heap_free();
    size_t min_free = e_heap_min_free();
    int num_atoms = free_count;
    mp_printf(&mp_plat_print,"Heap fragmentation: %d%%\n", fragmentation);
    mp_printf(&mp_plat_print,"Free bytes in Heap: %d\n", (int)free_bytes);
    mp_printf(&mp_plat_print,"Minimum free bytes in Heap: %d\n", (int)min_free);
    mp_printf(&mp_plat_print,"Number of free atoms: %d of %d\n", num_atoms,MAXATOMS);
	return mp_const_none;

}
static MP_DEFINE_CONST_FUN_OBJ_1(eigenmath_status_obj, eigenmath_status);



extern struct atom *zero;
static mp_obj_t eigenmath_del(mp_obj_t self_in) {
	mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	m_free(&self->pHeap); // deinitialize the hea
	zero = NULL;
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(eigenmath_del_obj, eigenmath_del);

extern struct atom *zero;
static mp_obj_t eigenmath_reset(mp_obj_t self_in) {
    mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	eigenmath_init(self->pHeap,self->heapSize);
    zero = NULL;//triger the symbol table initialization
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(eigenmath_reset_obj, eigenmath_reset);

mp_obj_t eigenmath_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] == MP_OBJ_NULL && attr == MP_QSTR___del__) {
        dest[0] = MP_OBJ_FROM_PTR(&eigenmath_del_obj);
        dest[1] = self_in;
    }else{
		// For any other attribute, indicate that lookup should continue in the locals dict
		dest[1] = MP_OBJ_SENTINEL;
		return MP_OBJ_NULL;
		}

    return MP_OBJ_NULL; 
}
static const mp_rom_map_elem_t eigenmath_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&eigenmath_run_obj) },
    { MP_ROM_QSTR(MP_QSTR_calc), MP_ROM_PTR(&eigenmath_calc_obj) },
    { MP_ROM_QSTR(MP_QSTR_runfile), MP_ROM_PTR(&eigenmath_runfile_obj) },
	{ MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&eigenmath_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&eigenmath_status_obj) },
};
static MP_DEFINE_CONST_DICT(eigenmath_locals_dict, eigenmath_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    eigenmath_type,
    MP_QSTR_EigenMath,
    MP_TYPE_FLAG_NONE,
    make_new, eigenmath_make_new,
    call,eigenmath_call, // call handler for the run method
    attr, eigenmath_attr,           // attr handler before locals_dict
    locals_dict, &eigenmath_locals_dict,
    print, eigenmath_print
);

static const mp_rom_map_elem_t eigenmath_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_eigenmath) },
    { MP_ROM_QSTR(MP_QSTR_EigenMath), MP_ROM_PTR(&eigenmath_type) },
    };
    static MP_DEFINE_CONST_DICT(mp_module_eigenmath_globals, eigenmath_module_globals_table);
    
    const mp_obj_module_t eigenmath_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_eigenmath_globals,
    };
    
    MP_REGISTER_MODULE(MP_QSTR_eigenmath, eigenmath_user_cmodule);