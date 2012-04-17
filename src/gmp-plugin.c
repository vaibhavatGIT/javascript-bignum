/* Incomplete, experimental browser (NPAPI) plug-in for multiple
   precision arithmetic.

   Copyright(C) 2012 John Tobey, see ../LICENCE
*/

#include <gmp.h>

#include <npapi.h>
#include <npfunctions.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#define PLUGIN_NAME        "GMP Arithmetic Library"
#define PLUGIN_DESCRIPTION PLUGIN_NAME " (EXPERIMENTAL)"
#define PLUGIN_VERSION     "0.0.0.0"

static NPNetscapeFuncs* sBrowserFuncs = NULL;

#define ENTRY(string, id) static NPIdentifier id;
#include "gmp-entries.h"

typedef struct _GmpInstance {
    NPObject npobj;
    NPP instance;
#define ENTRY(string, id) NPObject id ## _property;
#include "gmp-entries.h"
} GmpInstance;

#define GET_NPP(prop, id)                                               \
    (((GmpInstance*) (((char*) prop) -                                  \
                      offsetof (GmpInstance, id ## _property)))->instance)

static void
obj_noop (NPObject *npobj)
{
}

static bool
obj_id_false(NPObject *npobj, NPIdentifier name)
{
    return false;
}

static bool
obj_id_var_void(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
    VOID_TO_NPVARIANT (*result);
    return true;
}

static bool
setProperty_ro(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
    sBrowserFuncs->setexception (npobj, "read-only object");
    return true;
}

static bool
removeProperty_ro(NPObject *npobj, NPIdentifier name)
{
    sBrowserFuncs->setexception (npobj, "read-only object");
    return true;
}

static bool
enumerate_empty(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    *value = 0;
    *count = 0;
    return true;
}

typedef struct _Integer {
    NPObject npobj;
    mpz_t mpz;
} Integer;

static NPObject*
Integer_allocate(NPP npp, NPClass *aClass)
{
    Integer* z = (Integer*) sBrowserFuncs->memalloc (sizeof (Integer));
    if (z)
        mpz_init (z->mpz);
    return &z->npobj;
}

static void
Integer_deallocate(NPObject *npobj)
{
    if (npobj)
        mpz_clear (((Integer*) npobj)->mpz);
    sBrowserFuncs->memfree (npobj);
}

NPClass Integer_npclass = {
    structVersion   : NP_CLASS_STRUCT_VERSION,
    allocate        : Integer_allocate,
    deallocate      : Integer_deallocate,
    invalidate      : obj_noop,
    hasMethod       : obj_id_false,
    hasProperty     : obj_id_false,
    getProperty     : obj_id_var_void,
    setProperty     : setProperty_ro,
    removeProperty  : removeProperty_ro,
    enumerate       : enumerate_empty
};

static bool
isInteger (const NPVariant* arg)
{
    return NPVARIANT_IS_OBJECT (*arg)
        && NPVARIANT_TO_OBJECT (*arg)->_class == &Integer_npclass;
}

static Integer*
toInteger (const NPVariant* arg)
{
    return (Integer*) NPVARIANT_TO_OBJECT (*arg);
}

static bool
isNative (const NPVariant* arg)
{
    return NPVARIANT_IS_INT32 (*arg) || NPVARIANT_IS_DOUBLE (*arg);
}

static double
toDouble (const NPVariant* arg)
{
    if (NPVARIANT_IS_INT32 (*arg))
        return (double) NPVARIANT_TO_INT32 (*arg);
    return NPVARIANT_TO_DOUBLE (*arg);
}

static int32_t
toInt (const NPVariant* arg)
{
    if (NPVARIANT_IS_INT32 (*arg))
        return NPVARIANT_TO_INT32 (*arg);
    return (int32_t) NPVARIANT_TO_DOUBLE (*arg);
}

static bool
isFiniteDouble (const NPVariant* arg)
{
    double x;
    if (!NPVARIANT_IS_DOUBLE (*arg))
        return false;
    x = NPVARIANT_TO_DOUBLE (*arg);
    return x == x && finite (x);
}

static bool
isFiniteNative (const NPVariant* arg)
{
    return NPVARIANT_IS_INT32 (*arg) || isFiniteDouble (arg);
}

static bool
Integer_set_str (Integer* z, const NPString* str, int base)
{
    // XXX nul-terminated?
    return (base == 0 || (base >= 2 && base <= 62)) &&
        0 == mpz_set_str (z->mpz, str->UTF8Characters, base);
}

static bool
call_ID_mpz (NPObject *npobj,
             const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    NPP instance;
    Integer* ret;

    if (argCount != 0) {
        sBrowserFuncs->setexception (npobj, "invalid argument");
        return true;
    }

    instance = GET_NPP (npobj, ID_mpz);
    ret = (Integer*) sBrowserFuncs->createobject (instance, &Integer_npclass);

    if (ret)
        OBJECT_TO_NPVARIANT (&ret->npobj, *result);
    else
        sBrowserFuncs->setexception (npobj, "out of memory");
    return true;
}
/*
        bool type_ok = true;
        bool value_ok = true;
        // XXX mpz_set and friends will be separate entry points.
        if (argCount == 1 && NPVARIANT_IS_INT32 (args[0]))
            mpz_set_si (ret->mpz, NPVARIANT_TO_INT32 (args[0]));
        else if (argCount == 1 && isFiniteDouble (&args[0]))
            mpz_set_d (ret->mpz, NPVARIANT_TO_DOUBLE (args[0]));
        else if (argCount == 1 && isInteger (&args[0]))
            mpz_set (ret->mpz, toInteger (&args[0])->mpz);
        else if (argCount == 1 && NPVARIANT_IS_STRING (args[0]))
            value_ok = Integer_set_str (ret, &NPVARIANT_TO_STRING (args[0]), 0);
        else if (argCount == 2 && NPVARIANT_IS_STRING (args[0])
                 && isNative (&args[1]))
            value_ok = Integer_set_str (ret, &NPVARIANT_TO_STRING (args[0]),
                                        toInt (&args[1]));
        else if (argCount > 0)
            type_ok = false;
        if (type_ok) {
            if (value_ok)
            else
                sBrowserFuncs->setexception (npobj, "invalid argument");
        }
        else
            sBrowserFuncs->setexception (npobj, "wrong type arguments");
    }
 */

// XXX should be the toString method of Integer.
static bool
call_ID_mpz_get_str (NPObject *npobj, const NPVariant *args,
                     uint32_t argCount, NPVariant *result)
{
    int base = 0;
    if (argCount > 0 && isInteger (&args[0])) {
        if (argCount == 1)
            base = 10;
        else if (argCount == 2 && isNative (&args[1]))
            base = toInt (&args[1]);
    }
    if (base >= -36 && base <= 62 && base != 0 && base != -1 && base != 1) {
        Integer* z = toInteger (&args[0]);
        size_t len = mpz_sizeinbase (z->mpz, base) + 2;
        NPUTF8* s = sBrowserFuncs->memalloc (len);
        if (s) {
            mpz_get_str (s, base, z->mpz);
            if (s[0] != '-')
                len--;
            STRINGN_TO_NPVARIANT (s, s[len-2] ? len-1 : len-2, *result);
        }
        else
            sBrowserFuncs->setexception (npobj, "out of memory");
    }
    else
        sBrowserFuncs->setexception (npobj, "invalid argument");
    return true;
}

typedef unsigned long ulong;
typedef char const* char_ptr;
typedef int int_0_or_2_to_62;

static inline bool
in_mpz_ptr (const NPVariant* var, mpz_ptr* arg)
{
    if (!NPVARIANT_IS_OBJECT (*var)
        || NPVARIANT_TO_OBJECT (*var)->_class != &Integer_npclass)
        return false;
    *arg = &((Integer*) NPVARIANT_TO_OBJECT (*var))->mpz[0];
    return true;
}

static inline bool
in_long (const NPVariant* var, long* arg)
{
    if (NPVARIANT_IS_INT32 (*var))
        *arg = (long) NPVARIANT_TO_INT32 (*var);
    else if (isFiniteDouble (var))
        *arg = (long) NPVARIANT_TO_DOUBLE (*var);
    else
        return false;
    return true;
}

static inline bool
in_ulong (const NPVariant* var, ulong* arg)
{
    if (NPVARIANT_IS_INT32 (*var) && NPVARIANT_TO_INT32 (*var) >= 0)
        *arg = (ulong) NPVARIANT_TO_INT32 (*var);
    else if (isFiniteDouble (var) &&
        NPVARIANT_TO_DOUBLE (*var) == (ulong) NPVARIANT_TO_DOUBLE (*var))
        *arg = (ulong) NPVARIANT_TO_DOUBLE (*var);
    else
        return false;
    return true;
}

static inline bool
in_mp_bitcnt_t (const NPVariant* var, mp_bitcnt_t* arg)
{
    if (NPVARIANT_IS_INT32 (*var) && NPVARIANT_TO_INT32 (*var) >= 0)
        *arg = (mp_bitcnt_t) NPVARIANT_TO_INT32 (*var);
    else if (isFiniteDouble (var) &&
        NPVARIANT_TO_DOUBLE (*var) == (mp_bitcnt_t) NPVARIANT_TO_DOUBLE (*var))
        *arg = (mp_bitcnt_t) NPVARIANT_TO_DOUBLE (*var);
    else
        return false;
    return true;
}

static inline bool
in_int (const NPVariant* var, int* arg)
{
    if (NPVARIANT_IS_INT32 (*var))
        *arg = (int) NPVARIANT_TO_INT32 (*var);
    else if (isFiniteDouble (var))
        *arg = (int) NPVARIANT_TO_DOUBLE (*var);
    else
        return false;
    return true;
}

static inline bool
in_int_0_or_2_to_62 (const NPVariant* var, int* arg)
{
    return in_int (var, arg) && (*arg == 0 || (*arg >= 2 && *arg <= 62));
}

static inline bool
in_double (const NPVariant* var, double* arg)
{
    if (NPVARIANT_IS_DOUBLE (*var))
        *arg = NPVARIANT_TO_DOUBLE (*var);
    else if (NPVARIANT_IS_INT32 (*var))
        *arg = (double) NPVARIANT_TO_INT32 (*var);
    else
        return false;
    return true;
}

static inline bool
in_char_ptr (const NPVariant* var, char const** arg)
{
    if (!NPVARIANT_IS_STRING (*var))
        return false;
    *arg = NPVARIANT_TO_STRING (*var).UTF8Characters;
    return true;
}

static inline void
out_double (double value, NPVariant* result)
{
    DOUBLE_TO_NPVARIANT (value, *result);
}

static inline void
out_int (int value, NPVariant* result)
{
    if (value == (int32_t) value)
        INT32_TO_NPVARIANT (value, *result);
    else
        DOUBLE_TO_NPVARIANT ((double) value, *result);
}

static inline void
out_ulong (ulong value, NPVariant* result)
{
    if (value == (int32_t) value)
        INT32_TO_NPVARIANT (value, *result);
    else
        DOUBLE_TO_NPVARIANT ((double) value, *result);
}

static inline void
out_bool (int value, NPVariant* result)
{
    BOOLEAN_TO_NPVARIANT (value, *result);
}

#define ENTRY1(name, string, id, rett, t0)                              \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    if (argCount == 1                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        )                                                               \
        out_ ## rett (name (a0), result);                               \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY2v(name, string, id, t0, t1)                               \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    if (argCount == 2                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        )                                                               \
    {                                                                   \
        name (a0, a1);                                                  \
        VOID_TO_NPVARIANT (*result);                                    \
    }                                                                   \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY2(name, string, id, rett, t0, t1)                          \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    if (argCount == 2                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        )                                                               \
        out_ ## rett (name (a0, a1), result);                           \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY3v(name, string, id, t0, t1, t2)                           \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    t2 a2;                                                              \
    if (argCount == 3                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        && in_ ## t2 (&args[2], &a2)                                    \
        )                                                               \
    {                                                                   \
        name (a0, a1, a2);                                              \
        VOID_TO_NPVARIANT (*result);                                    \
    }                                                                   \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY3(name, string, id, rett, t0, t1, t2)                      \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    t2 a2;                                                              \
    if (argCount == 3                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        && in_ ## t2 (&args[2], &a2)                                    \
        )                                                               \
        out_ ## rett (name (a0, a1, a2), result);                       \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY4v(name, string, id, t0, t1, t2, t3)                       \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    t2 a2;                                                              \
    t3 a3;                                                              \
    if (argCount == 4                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        && in_ ## t2 (&args[2], &a2)                                    \
        && in_ ## t3 (&args[3], &a3)                                    \
        )                                                               \
    {                                                                   \
        name (a0, a1, a2, a3);                                          \
        VOID_TO_NPVARIANT (*result);                                    \
    }                                                                   \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#define ENTRY4(name, string, id, rett, t0, t1, t2, t3)                  \
static bool                                                             \
call_ ## id (NPObject *npobj,                                           \
             const NPVariant *args, uint32_t argCount, NPVariant *result) \
{                                                                       \
    t0 a0;                                                              \
    t1 a1;                                                              \
    t2 a2;                                                              \
    t3 a3;                                                              \
    if (argCount == 4                                                   \
        && in_ ## t0 (&args[0], &a0)                                    \
        && in_ ## t1 (&args[1], &a1)                                    \
        && in_ ## t2 (&args[2], &a2)                                    \
        && in_ ## t3 (&args[3], &a3)                                    \
        )                                                               \
        out_ ## rett (name (a0, a1, a2, a3), result);                   \
    else                                                                \
        sBrowserFuncs->setexception (npobj, "wrong type arguments");    \
    return true;                                                        \
}

#include "gmp-entries.h"

// XXX I'd like to do mpz_get_d_2exp, but creating an array to hold
// the two results will be a mild pain.

#define ENTRY(string, id)                               \
    static NPClass id ## _npclass = {                   \
        structVersion   : NP_CLASS_STRUCT_VERSION,      \
        deallocate      : obj_noop,                     \
        invalidate      : obj_noop,                     \
        hasMethod       : obj_id_false,                 \
        invokeDefault   : call_ ## id,                  \
        hasProperty     : obj_id_false,                 \
        getProperty     : obj_id_var_void,              \
        setProperty     : setProperty_ro,               \
        removeProperty  : removeProperty_ro,            \
        enumerate       : enumerate_empty               \
    };
#include "gmp-entries.h"

static NPObject*
gmp_allocate (NPP instance, NPClass *aClass)
{
    GmpInstance* ret = (GmpInstance*)
        sBrowserFuncs->memalloc (sizeof (GmpInstance));
    if (ret) {
        ret->instance = instance;
#define ENTRY(string, id)                                \
        ret->id ## _property._class = &id ## _npclass;   \
        ret->id ## _property.referenceCount = 1;
#include "gmp-entries.h"
    }
    return &ret->npobj;
}

static void
gmp_deallocate (NPObject *npobj)
{
    sBrowserFuncs->memfree (npobj);
}

static bool
gmp_invoke(NPObject *npobj, NPIdentifier name,
           const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    sBrowserFuncs->setexception (npobj,
                                 "oops, browser used invoke instead of getProperty");
    return true;
}

static bool
gmp_hasProperty(NPObject *npobj, NPIdentifier key)
{
    return false
#define ENTRY(string, id) || key == id
#include "gmp-entries.h"
        ;
}

static bool
gmp_getProperty(NPObject *npobj, NPIdentifier key, NPVariant *result)
{
    GmpInstance* gmpinst = (GmpInstance*) npobj;
    NPObject* func = 0;
    if (0 == 1)
        func = func;
#define ENTRY(string, id) else if (key == id)   \
        func = &gmpinst->id ## _property;
#include "gmp-entries.h"
    if (func)
        OBJECT_TO_NPVARIANT (sBrowserFuncs->retainobject (func), *result);
    else
        VOID_TO_NPVARIANT (*result);
    return true;
}

static bool
gmp_enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    uint32_t cnt = 0
#define ENTRY(string, id) +1
#include "gmp-entries.h"
        ;
    *value = sBrowserFuncs->memalloc (cnt * sizeof (NPIdentifier*));
    *count = cnt;
    cnt = 0;
#define ENTRY(string, id) (*value)[cnt++] = id;
#include "gmp-entries.h"
    return true;
}

static NPClass gmp_npclass = {
    structVersion   : NP_CLASS_STRUCT_VERSION,
    allocate        : gmp_allocate,
    deallocate      : gmp_deallocate,
    invalidate      : obj_noop,
    hasMethod       : obj_id_false,
    invoke          : gmp_invoke,
    hasProperty     : gmp_hasProperty,
    getProperty     : gmp_getProperty,
    setProperty     : setProperty_ro,
    removeProperty  : removeProperty_ro,
    enumerate       : gmp_enumerate
};

static NPError
npp_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
        int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  // Make this a windowless plug-in.  This makes Chrome happy.
    sBrowserFuncs->setvalue (instance, NPPVpluginWindowBool, (void*) false);
    instance->pdata = sBrowserFuncs->createobject (instance, &gmp_npclass);
    if (!instance->pdata)
        return NPERR_OUT_OF_MEMORY_ERROR;
    return NPERR_NO_ERROR;
}

static NPError
npp_Destroy(NPP instance, NPSavedData** save) {
    void* data = instance->pdata;
    instance->pdata = 0;
    if (data)
        sBrowserFuncs->releaseobject ((NPObject*) data);
    return NPERR_NO_ERROR;
}

static NPError
npp_GetValue(NPP instance, NPPVariable variable, void *value) {
    switch (variable) {
    case NPPVpluginScriptableNPObject:
        *((NPObject**)value) =
            sBrowserFuncs->retainobject ((NPObject*) instance->pdata);
        break;
    default:
        return NPERR_GENERIC_ERROR;
    }
    return NPERR_NO_ERROR;
}

NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
    sBrowserFuncs = bFuncs;

    // Check the size of the provided structure based on the offset of the
    // last member we need.
    if (pFuncs->size < (offsetof(NPPluginFuncs, getvalue) + sizeof(void*)))
        return NPERR_INVALID_FUNCTABLE_ERROR;

    pFuncs->newp = npp_New;
    pFuncs->destroy = npp_Destroy;
    pFuncs->getvalue = npp_GetValue;

#define ENTRY(str, id) id = sBrowserFuncs->getstringidentifier (str);
#include "gmp-entries.h"

    return NPERR_NO_ERROR;
}

NP_EXPORT(char*)
NP_GetPluginVersion()
{
    return PLUGIN_VERSION;
}

NP_EXPORT(const char*)
NP_GetMIMEDescription()
{
    return "application/x-gmplib:gmp:GNU Multiple Precision Arithmetic Library";
}

NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
    switch (aVariable) {
    case NPPVpluginNameString:
        *((char**)aValue) = PLUGIN_NAME;
        break;
    case NPPVpluginDescriptionString:
        *((char**)aValue) = PLUGIN_DESCRIPTION;
        break;
    default:
        return NPERR_INVALID_PARAM;
        break;
    }
    return NPERR_NO_ERROR;
}

NP_EXPORT(NPError)
NP_Shutdown()
{
    return NPERR_NO_ERROR;
}
