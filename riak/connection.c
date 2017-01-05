/*
   Copyright 2013 Trifork A/S
   Author: Kaspar Bach Pedersen

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

#include "connection.h"
#include "object.h"
#include "bucket.h"
#include "exception/exception.h"
#include "server_info.h"
#include "zend_interfaces.h"
#include <zend_exceptions.h>

zend_class_entry *riak_connection_ce;

ZEND_BEGIN_ARG_INFO_EX(arginfo_riak_connection_ctor, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_riak_connection_noargs, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_riak_connection_get_bucket, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

static zend_function_entry riak_connection_methods[] = {
    PHP_ME(RiakConnection, __construct, arginfo_riak_connection_ctor, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(RiakConnection, ping, arginfo_riak_connection_noargs, ZEND_ACC_PUBLIC)
    PHP_ME(RiakConnection, getHost, arginfo_riak_connection_noargs, ZEND_ACC_PUBLIC)
    PHP_ME(RiakConnection, getPort, arginfo_riak_connection_noargs, ZEND_ACC_PUBLIC)
    PHP_ME(RiakConnection, getBucket, arginfo_riak_connection_get_bucket, ZEND_ACC_PUBLIC)
    PHP_ME(RiakConnection, getServerInfo, arginfo_riak_connection_noargs, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void riak_connection_init(TSRMLS_D) /* {{{ */
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Riak", "Connection", riak_connection_methods);

    ce.create_object = create_client_data;
    riak_connection_ce = zend_register_internal_class(&ce TSRMLS_CC);

    zend_declare_property_null(riak_connection_ce, "host", sizeof("host")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(riak_connection_ce, "port", sizeof("port")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
}
/* }}} */

zend_object *create_client_data(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
    zend_object_handlers client_data_handlers;
    client_data *tobj;

    tobj = ecalloc(1, sizeof(client_data) + zend_object_properties_size(class_type));

    zend_object_std_init((zend_object *) &tobj->std, class_type TSRMLS_CC);

    client_data_handlers.offset = XtOffsetOf(struct tobj, std);
    client_data_handlers.free_obj = free_client_data;

    tobj->std.handlers = client_data_handlers;

    return &tobj->std;
}
/* }}} */

void free_client_data(void *object TSRMLS_DC) /* {{{ */
{
    client_data *data;

    data = (client_data *) ((char *) object - XtOffsetOf(client_data, std));

    zend_object_std_dtor(&data->std TSRMLS_CC);

    if (data->connection) {
        release_connection(data->connection TSRMLS_CC);
    }

    efree(data);
}
/* }}} */

void create_client_object(zval *zclient, char *host, long port TSRMLS_DC) /* {{{ */
{
    zval zhost, zport;

    ZVAL_STRING(&zhost, host, 1);
    ZVAL_LONG(&zport, port);

    object_init_ex(zclient, riak_connection_ce);
    RIAK_CALL_METHOD2(RiakConnection, __construct, zclient, zclient, &zhost, &zport);

    zval_dtor(&zhost);
    zval_dtor(&zport);
}
/* }}} */

riak_connection *get_client_connection(zval *zclient TSRMLS_DC)/* {{{ */
{
    client_data *data;

    data = (client_data *) ((char *) zclient - XtOffsetOf(client_data, std));

    if (data->connection) {
        ensure_connected(data->connection TSRMLS_CC);

        return data->connection;
    }

    if ( ! create_object_connection(zclient TSRMLS_CC)) {
        return NULL;
    }

    return data->connection;
}
/* }}} */

int create_object_connection(zval *zclient TSRMLS_DC)/* {{{ */
{
    client_data *data;
    zval zhost, zport;

    zend_call_method_with_0_params(&zclient, NULL, NULL, "getHost", &zhost);
    zend_call_method_with_0_params(&zclient, NULL, NULL, "getPort", &zport);

    data = (client_data *) ((char *) zclient - XtOffsetOf(client_data, std));
    data->connection = take_connection(Z_STRVAL(zhost), Z_STRLEN(zhost), Z_LVAL(zport) TSRMLS_CC);

    zval_dtor(&zhost);
    zval_dtor(&zport);

    if ( ! data->connection) {
        return 0;
    }

    return 1;
}
/* }}} */

/* {{{ proto void RiakConnection->__construct(string $host, [int $port])
Create a new RiakConnection */
PHP_METHOD(RiakConnection, __construct)
{
    char *host;
    size_t hostLen;
    zend_long port = DEFAULT_PORT;
    zval zbucketarr;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &host, &hostLen, &port) == FAILURE) {
        zend_throw_exception(riak_badarguments_exception_ce, "Bad or missing argument", 500 TSRMLS_CC);
        return;
    }

    zend_update_property_stringl(riak_connection_ce, getThis(), "host", sizeof("host")-1, host, hostLen TSRMLS_CC);
    zend_update_property_long(riak_connection_ce, getThis(), "port", sizeof("port")-1, port TSRMLS_CC);

    array_init(&zbucketarr);
    zend_update_property(riak_connection_ce, getThis(), "buckets", sizeof("buckets")-1, &zbucketarr TSRMLS_CC);
    zval_dtor(&zbucketarr);
}
/* }}} */

/* {{{ proto void Riak\Connection->getServerInfo()
Get a ServerInfo object */
PHP_METHOD(RiakConnection, getServerInfo)
{
    zval sinfo;

    object_init_ex(&sinfo, riak_server_info_ce);
    RIAK_CALL_METHOD1(Riak_Server_Info, __construct, NULL, sinfo, getThis());

    RETURN_ZVAL(&sinfo, 0, 1);
}
/* }}} */

/* {{{ proto void Riak\Connection->ping()
Ping riak to see if it is alive, an exception is thrown if no response is received */
PHP_METHOD(RiakConnection, ping)
{
    int pingStatus;
    riak_connection *connection;

    connection = get_client_connection(getThis() TSRMLS_CC);

    THROW_EXCEPTION_IF_CONNECTION_IS_NULL(connection);

    pingStatus = riack_ping(connection->client);

    CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, pingStatus);
}
/* }}} */

/* {{{ proto string Riak\Connection->getHost()
Get this connections hostname */
PHP_METHOD(RiakConnection, getHost)
{
    RIAK_GETTER_STRING(riak_connection_ce, "host");
}
/* }}} */

/* {{{ proto int Riak\Connection->getPort()
Get this connections port number */
PHP_METHOD(RiakConnection, getPort)
{
    RIAK_GETTER_LONG(riak_connection_ce, "port");
}
/* }}} */

/* {{{ proto Riak\Bucket Riak\Connection->getBucket(string $bucketName)
Get a Riak\Bucket */
PHP_METHOD(RiakConnection, getBucket)
{
    char *name;
    size_t name_len;
    zval *zbucketarr, *zbucket;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
        zend_throw_exception(riak_badarguments_exception_ce, "Bad or missing argument", 500 TSRMLS_CC);
        return;
    }

    // Se if we have a bucket with that name already
    zbucketarr = zend_read_property(riak_connection_ce, getThis(), "buckets", sizeof("buckets")-1, 1 TSRMLS_CC);

    if (Z_TYPE_P(zbucketarr) == IS_ARRAY) {
        zval ztmp;

        if ((ztmp = zend_hash_find(Z_ARRVAL_P(zbucketarr), name)) != NULL) {
            RETURN_ZVAL(&ztmp, 1, 0);
        }
    }

    // If we are here we did not find an existing bucket, create a new
    zbucket = create_bucket_object(getThis(), name, name_len TSRMLS_CC);

    RETURN_ZVAL(zbucket, 0, 1);
}
/* }}} */
