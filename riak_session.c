/*
   Copyright 2012 Trifork A/S
   Author: Kaspar Pedersen

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "riak_session.h"

#ifdef PHP_RIAK_SESSION

#include <zend_exceptions.h>
#include "riak/connection.h"
#include "riak/bucket.h"
#include "riak/object.h"
#include "riak/object_list.h"
#include "riak/input/delete_input.h"
#include "riak/input/get_input.h"
#include "riak/input/put_input.h"
#include "riak/output/output.h"
#include "SAPI.h"
#include <ext/standard/php_string.h>
#include <ext/standard/url.h>
#include <main/php_variables.h>

#define SET_GET_OPTION_IF_PRESENT(NAME, CLIENT, ZARR, ZPP) \
    if ((ZPP = zend_hash_find(Z_ARRVAL(ZARR), NAME)) != NULL) { \
        convert_to_long(*ZPP); \
        zend_update_property(riak_get_input_ce, CLIENT, NAME, sizeof(NAME)-1, *ZPP TSRMLS_CC); \
    }

#define SET_PUT_OPTION_IF_PRESENT(NAME, CLIENT, ZARR, ZPP) \
    if ((ZPP = zend_hash_find(Z_ARRVAL(ZARR), NAME)) != NULL) { \
        convert_to_long(*ZPP); \
        zend_update_property(riak_put_input_ce, CLIENT, NAME, sizeof(NAME)-1, *ZPP TSRMLS_CC); \
    }

#define SET_DELETE_OPTION_IF_PRESENT(NAME, CLIENT, ZARR, ZPP) \
    if ((ZPP = zend_hash_find(Z_ARRVAL(ZARR), NAME)) != NULL) { \
        convert_to_long(*ZPP); \
        zend_update_property(riak_delete_input_ce, CLIENT, NAME, sizeof(NAME)-1, *ZPP TSRMLS_CC); \
    }

typedef struct _riak_session_data {
    char* session_name;
    zval zbucket;
    zval zclient;
    zval zgetprops;
    zval zputprops;
    zval zdelprops;
} riak_session_data;

PS_OPEN_FUNC(riak);
PS_CLOSE_FUNC(riak);
PS_READ_FUNC(riak);
PS_WRITE_FUNC(riak);
PS_DESTROY_FUNC(riak);
PS_GC_FUNC(riak);

ps_module ps_mod_riak = {
    PS_MOD(riak)
};

#define PS_RIAK_DATA riak_session_data *data = PS_GET_MOD_DATA()

PS_OPEN_FUNC(riak) /* {{{ */
{
    riak_session_data* data;
    zval zclient, zbucket, zgetprops, zdelprops, zputprops, zoptionsarray, zoption;
    php_url *purl;
    char* stripped_path;

    purl = php_url_parse(save_path);

    if ( ! purl) {
        goto cleanup;
    }

    data = ecalloc(1, sizeof(riak_session_data));

    if ( ! data) {
        goto cleanup_purl;
    }

    create_client_object(&data->zclient, purl->host, purl->port TSRMLS_CC);

    if (EG(exception)) {
        goto cleanup_client;
    }

    // Input options for get operations
    object_init_ex(&data->zgetprops, riak_get_input_ce);
    object_init_ex(&data->zputprops, riak_put_input_ce);
    object_init_ex(&data->zdelprops, riak_delete_input_ce);

    /* Set w,dw,pw,r,rw,pw if set */
    array_init(&zoptionsarray);

    if (purl->query) {
        sapi_module.treat_data(PARSE_STRING, estrdup(purl->query), &zoptionsarray TSRMLS_CC);

        SET_GET_OPTION_IF_PRESENT("r", &data->zgetprops, zoptionsarray, zoption)
        SET_GET_OPTION_IF_PRESENT("rw", &data->zgetprops, zoptionsarray, zoption)
        SET_GET_OPTION_IF_PRESENT("pr", &data->zgetprops, zoptionsarray, zoption)

        SET_PUT_OPTION_IF_PRESENT("w", &data->zputprops, zoptionsarray, zoption)
        SET_PUT_OPTION_IF_PRESENT("dw", &data->zputprops, zoptionsarray, zoption)
        SET_PUT_OPTION_IF_PRESENT("pw", &data->zputprops, zoptionsarray, zoption)

        SET_DELETE_OPTION_IF_PRESENT("r", &data->zdelprops, zoptionsarray, zoption)
        SET_DELETE_OPTION_IF_PRESENT("pr", &data->zdelprops, zoptionsarray, zoption)
        SET_DELETE_OPTION_IF_PRESENT("rw", &data->zdelprops, zoptionsarray, zoption)
        SET_DELETE_OPTION_IF_PRESENT("w", &data->zdelprops, zoptionsarray, zoption)
        SET_DELETE_OPTION_IF_PRESENT("dw", &data->zdelprops, zoptionsarray, zoption)
        SET_DELETE_OPTION_IF_PRESENT("pw", &data->zdelprops, zoptionsarray, zoption)
    }

    zval_dtor(&zoptionsarray);

    stripped_path = php_trim(purl->path, strlen(purl->path), "/", 1, NULL, 3 TSRMLS_CC);

    create_bucket_object(&data->bucket, &data->zclient, stripped_path, strlen(stripped_path) TSRMLS_CC);

    efree(stripped_path);

    if (EG(exception)) {
        goto cleanup_zvals;
    }

    php_url_free(purl);

    data->session_name = estrdup(session_name);

    PS_SET_MOD_DATA(data);

    return SUCCESS;

cleanup_zvals:
    zval_dtor(&data->zgetprops);
    zval_dtor(&data->zputprops);
    zval_dtor(&data->zdelprops);
    zval_dtor(&data->zbucket);

cleanup_client:
    zval_dtor(&data->zclient);

    efree(data);

cleanup_purl:
    php_url_free(purl);

    PS_SET_MOD_DATA(NULL);

cleanup:
    return FAILURE;
}
/* }}} */

PS_CLOSE_FUNC(riak) /* {{{ */
{
    riak_session_data *data = PS_GET_MOD_DATA();

    if (data) {
        efree(data->session_name);

        zval_ptr_dtor(&data->zbucket);
        zval_ptr_dtor(&data->zclient);
        zval_ptr_dtor(&data->zgetprops);
        zval_ptr_dtor(&data->zputprops);
        zval_ptr_dtor(&data->zdelprops);

        efree(data);

        PS_SET_MOD_DATA(NULL);
    }

    return SUCCESS;
}
/* }}} */

PS_READ_FUNC(riak) /* {{{ */
{
    zval zoutput, zkey, *zdata;
    riak_session_data *data = PS_GET_MOD_DATA();
    *vallen = 0;

    ZVAL_STRING(&zkey, key, 1);

    object_init_ex(&zoutput, riak_output_ce);

    RIAK_CALL_METHOD2(RiakBucket, get, zoutput, data->zbucket, zkey, data->zgetprops);

    if (EG(exception)) {
        zend_clear_exception(TSRMLS_C);
    } else if (Z_TYPE_P(zoutput) == IS_OBJECT) {
        zval zobjlist;

        RIAK_CALL_METHOD(Riak_Output_Output, getObjectList, &zobjlist, zoutput);
        if (Z_TYPE(zobjlist) == IS_OBJECT) {
            zval zobject;
            RIAK_CALL_METHOD(Riak_Object_List, first, &zobject, &zobjlist);
            if (Z_TYPE(zobject) == IS_OBJECT) {
                zdata = zend_read_property(riak_object_ce, &zobject, "content", sizeof("content")-1, 1 TSRMLS_CC);
                if (zdata->type == IS_STRING && Z_STRLEN_P(zdata) > 0) {
                    *vallen = Z_STRLEN_P(zdata);
                    *val = emalloc(Z_STRLEN_P(zdata));
                    memcpy(*val, Z_STRVAL_P(zdata), Z_STRLEN_P(zdata));
                }
            }
            zval_dtor(&zobject);
        }
        zval_dtor(&zobjlist);
    }

    if (*vallen == 0) {
        *val = STR_EMPTY_ALLOC();
    }

    zval_ptr_dtor(&zoutput);
    zval_ptr_dtor(&zkey);

    return SUCCESS;
}
/* }}} */

PS_WRITE_FUNC(riak) /* {{{ */
{
    zval *zobject = create_object_object(key TSRMLS_CC);
    riak_session_data *data = PS_GET_MOD_DATA();

    if (EG(exception)) {
        zend_clear_exception(TSRMLS_C);
        zval_ptr_dtor(&zobject);

        return FAILURE;
    }

    zend_update_property_stringl(riak_object_ce, zobject, "content", sizeof("content")-1, val, vallen TSRMLS_CC);
    RIAK_CALL_METHOD2(RiakBucket, put, zobject, data->zbucket, zobject, data->zputprops);
    zval_ptr_dtor(&zobject);

    if (EG(exception)) {
        zend_clear_exception(TSRMLS_C);
        return FAILURE;
    }

    return SUCCESS;
}
/* }}} */

PS_DESTROY_FUNC(riak) /* {{{ */
{
    riak_session_data *data = PS_GET_MOD_DATA();
    zval *zobject = create_object_object(key TSRMLS_CC);

    RIAK_CALL_METHOD2(RiakBucket, delete, zobject, data->zbucket, zobject, data->zdelprops);
    zval_ptr_dtor(&zobject);

    if (EG(exception)) {
        zend_clear_exception(TSRMLS_C);
        return FAILURE;
    }

    return SUCCESS;
}

PS_GC_FUNC(riak) /* {{{ */
{
    /* Do nothing riak´s builtin key expire should be used on the session bucket */
    return SUCCESS;
}
/* }}} */

#endif // PHP_RIAK_SESSION
