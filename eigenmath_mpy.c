
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
#include "py/gc.h"
#include "eigenmath.h"



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
	
	self->base.type = type;
    self->heapSize = mp_obj_get_int(args[0]); // 350 * 1024; // 350KB
    
	//STACKSIZE  = mp_obj_get_int(args[1]);
	//MAXATOMS = mp_obj_get_int(args[2]); // 512*20
	//uint32_t sizeOfpAtom = sizeof(struct atom *);	
    self->pHeap = m_malloc(self->heapSize);
	if (self->pHeap == NULL){
		mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to initialize heap"));
		return MP_OBJ_NULL;
	}; // initialize heap with 300000 bytes
	//pHeap = &self->heap; // set global heap pointer

    eigenmath_init(self->pHeap,self->heapSize);

	return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t eigenmath_run(mp_obj_t self_in, mp_obj_t input_str_obj) {
	//mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	mp_check_self(mp_obj_is_str_or_bytes(input_str_obj));
	//const char *str;
	//size_t str_len;
	GET_STR_DATA_LEN(input_str_obj, str, str_len);
	//mp_printf(&mp_plat_print, "debug input str is %s",str); 
	
	//char *cmdBuffer = (char *)m_malloc(str_len+1);
	char *cmdBuffer = (char *)m_malloc(pHeap, str_len+1); // allocate memory for command buffer
	if (cmdBuffer == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to allocate memory for command"));
    }
	memcpy(cmdBuffer, str, str_len);
	cmdBuffer[str_len] = '\0';
	//mp_printf(&mp_plat_print, "debug input str is %s",cmdBuffer); 
	
	//mp_obj_t result;
	

	//outbuf[0] = '\0';
	run(cmdBuffer);
	//if (outbuf_index == 0 || strlen((const char *)outbuf) == 0) {
        //m_free(cmdBuffer);
    //    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Eigenmath execution failed"));
    //}
	//result = mp_obj_new_str(outbuf, strlen((const char *)outbuf));
	//m_free(cmdBuffer);

	return mp_const_none;

}
static MP_DEFINE_CONST_FUN_OBJ_2(eigenmath_run_obj, eigenmath_run);

static mp_obj_t eigenmath_del(mp_obj_t self_in) {
	mp_obj_eigenmath_t *self = MP_OBJ_TO_PTR(self_in);
	m_free(&self->pHeap); // deinitialize the hea
	
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(eigenmath_del_obj, eigenmath_del);


static mp_obj_t eigenmath_reset(mp_obj_t self_in) {
	
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
	{ MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&eigenmath_reset_obj) },
};
static MP_DEFINE_CONST_DICT(eigenmath_locals_dict, eigenmath_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    eigenmath_type,
    MP_QSTR_EigenMath,
    MP_TYPE_FLAG_NONE,
    make_new, eigenmath_make_new,
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