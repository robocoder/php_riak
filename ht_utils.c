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

#include "ht_utils.h"

void foreach_in_hashtable(void* callingObj, void* custom_ptr, HashTable *ht, ht_entry_callback cb TSRMLS_DC)
{
    ulong num_key;
    zend_string *key;
    zval *zv;
    int cnt = 0;

    ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, key, zv) {
        if (key) { // HASH_KEY_IS_STRING
            (*cb)(callingObj, custom_ptr, key, 0, zv, &cnt TSRMLS_CC);
        } else  {  // HASH_KEY_IS_LONG
            (*cb)(callingObj, custom_ptr, NULL, num_key, zv, &cnt TSRMLS_CC);
        }

        cnt++;
    } ZEND_HASH_FOREACH_END();
}
