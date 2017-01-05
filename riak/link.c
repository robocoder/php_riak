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

#include "link.h"

zend_class_entry *riak_link_ce;

ZEND_BEGIN_ARG_INFO_EX(arginfo_link_ctor, 0, ZEND_RETURN_VALUE, 3)
    ZEND_ARG_INFO(0, tag)
    ZEND_ARG_INFO(0, bucket)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_link_noargs, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry riak_link_methods[] = {
    PHP_ME(RiakLink, __construct, arginfo_link_ctor, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(RiakLink, getTag, arginfo_link_noargs, ZEND_ACC_PUBLIC)
    PHP_ME(RiakLink, getBucketName, arginfo_link_noargs, ZEND_ACC_PUBLIC)
    PHP_ME(RiakLink, getKey, arginfo_link_noargs, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void riak_link_init(TSRMLS_D)/* {{{ */
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Riak", "Link", riak_link_methods);
    riak_link_ce = zend_register_internal_class(&ce TSRMLS_CC);

    zend_declare_property_null(riak_link_ce, "tag", sizeof("tag")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(riak_link_ce, "bucket", sizeof("bucket")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(riak_link_ce, "key", sizeof("key")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
}
/* }}} */

void create_link_object(zval *zlink, const char *tag, const char *bucket, const char *key TSRMLS_DC)/* {{{ */
{
    zval zbucket, ztag, zkey;

    ZVAL_STRING(&zkey, key, 1);
    ZVAL_STRING(&zbucket, bucket, 1);
    ZVAL_STRING(&ztag, tag, 1);

    object_init_ex(zlink, riak_link_ce);
    RIAK_CALL_METHOD3(RiakLink, __construct, zlink, zlink, &ztag, &zbucket, &zkey);

    zval_dtor(&zkey);
    zval_dtor(&zbucket);
    zval_dtor(&ztag);
}
/* }}} */

/* {{{ proto void Riak\Link->__construct(string $tag, string $bucket, string $key)
Create a new link */
PHP_METHOD(RiakLink, __construct)
{
    char *key, *bucket, *tag;
    size_t keylen, bucketlen, taglen;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &tag, &taglen, &bucket, &bucketlen, &key, &keylen) == FAILURE) {
        return;
    }

    zend_update_property_stringl(riak_link_ce, getThis(), "tag", sizeof("tag")-1, tag, taglen TSRMLS_CC);
    zend_update_property_stringl(riak_link_ce, getThis(), "bucket", sizeof("bucket")-1, bucket, bucketlen TSRMLS_CC);
    zend_update_property_stringl(riak_link_ce, getThis(), "key", sizeof("key")-1, key, keylen TSRMLS_CC);
}
/* }}} */

/* {{{ proto string Riak\Link->getTag() */
PHP_METHOD(RiakLink, getTag)
{
    RIAK_GETTER_STRING(riak_link_ce, "tag");
}
/* }}} */

/* {{{ proto string Riak\Link->getBucketName() */
PHP_METHOD(RiakLink, getBucketName)
{
    RIAK_GETTER_STRING(riak_link_ce, "bucket");
}
/* }}} */

/* {{{ proto string Riak\Link->getKey() */
PHP_METHOD(RiakLink, getKey)
{
    RIAK_GETTER_STRING(riak_link_ce, "key");
}
/* }}} */
