/*
  +----------------------------------------------------------------------+
  | SeasLog                                                              |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Neeke.Gao  <neeke@php.net>                                   |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"
#include "ext/date/php_date.h"
#include "php_seaslog.h"
#include "zend_extensions.h"
#include <sys/resource.h>
#include <stdlib.h>

#ifdef PHP_WIN32
#include "win32/time.h"
#elif defined(NETWARE)
#include <sys/timeval.h>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif

void seaslog_init_logger(TSRMLS_D);
static int seaslog_shutdown_buffer(TSRMLS_D);

ZEND_DECLARE_MODULE_GLOBALS(seaslog)

static int le_seaslog;
static char *last_logger = "default";
static char *base_path = "";
static zend_bool disting_type = 0;
static zend_bool disting_by_hour = 0;
static zend_bool use_buffer = 0;
static int buffer_size = 0;
static int level = 0;


const zend_function_entry seaslog_functions[] = {
    PHP_FE(seaslog_get_version,	NULL)
    PHP_FE(seaslog_get_author,	NULL)
    {NULL, NULL, NULL}
};

const zend_function_entry seaslog_methods[] = {
        PHP_ME(SEASLOG_RES_NAME, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
        PHP_ME(SEASLOG_RES_NAME, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
        PHP_ME(SEASLOG_RES_NAME, setBasePath, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, getBasePath, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, setLogger, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, getLastLogger, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, analyzerCount, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, analyzerDetail, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, getBuffer, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

        PHP_ME(SEASLOG_RES_NAME, log, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, debug, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, info, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, notice, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, warning, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, error, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, critical, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, alert, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
        PHP_ME(SEASLOG_RES_NAME, emergency, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

        PHP_FE_END
};

zend_module_entry seaslog_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
    SEASLOG_RES_NAME,
    seaslog_functions,
    PHP_MINIT(seaslog),
    PHP_MSHUTDOWN(seaslog),
    PHP_RINIT(seaslog),
    PHP_RSHUTDOWN(seaslog),
    PHP_MINFO(seaslog),
#if ZEND_MODULE_API_NO >= 20010901
    SEASLOG_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SEASLOG
ZEND_GET_MODULE(seaslog)
#endif

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("seaslog.default_basepath", "/log", PHP_INI_ALL, OnUpdateString, default_basepath, zend_seaslog_globals, seaslog_globals)
    STD_PHP_INI_ENTRY("seaslog.default_logger", "default", PHP_INI_ALL, OnUpdateString, default_logger, zend_seaslog_globals, seaslog_globals)
    STD_PHP_INI_ENTRY("seaslog.logger", "default", PHP_INI_ALL, OnUpdateString, logger, zend_seaslog_globals, seaslog_globals)
    STD_PHP_INI_BOOLEAN("seaslog.disting_type", "0", PHP_INI_ALL, OnUpdateBool, disting_type, zend_seaslog_globals, seaslog_globals)
    STD_PHP_INI_BOOLEAN("seaslog.disting_by_hour","0", PHP_INI_ALL,OnUpdateBool, disting_by_hour, zend_seaslog_globals, seaslog_globals)
    STD_PHP_INI_BOOLEAN("seaslog.use_buffer","0",PHP_INI_ALL,OnUpdateBool,use_buffer,zend_seaslog_globals,seaslog_globals)
    STD_PHP_INI_ENTRY("seaslog.buffer_size","0",PHP_INI_ALL,OnUpdateLongGEZero,buffer_size,zend_seaslog_globals,seaslog_globals)
    STD_PHP_INI_ENTRY("seaslog.level","0",PHP_INI_ALL,OnUpdateLongGEZero,level,zend_seaslog_globals,seaslog_globals)
PHP_INI_END()


static void php_seaslog_init_globals(zend_seaslog_globals *seaslog_globals)
{

}


PHP_MINIT_FUNCTION(seaslog)
{
    REGISTER_INI_ENTRIES();

    REGISTER_STRINGL_CONSTANT("SEASLOG_VERSION", SEASLOG_VERSION, 	sizeof(SEASLOG_VERSION) - 1, 	CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_AUTHOR", SEASLOG_AUTHOR, 	sizeof(SEASLOG_AUTHOR) - 1, 	CONST_PERSISTENT | CONST_CS);

    REGISTER_STRINGL_CONSTANT("SEASLOG_DEBUG",     SEASLOG_DEBUG, 	  sizeof(SEASLOG_DEBUG) - 1,    CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_INFO",      SEASLOG_INFO, 	  sizeof(SEASLOG_INFO) - 1,     CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_NOTICE",    SEASLOG_NOTICE, 	  sizeof(SEASLOG_NOTICE) - 1,   CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_WARNING",   SEASLOG_WARNING,   sizeof(SEASLOG_WARNING) - 1,  CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_ERROR",     SEASLOG_ERROR, 	  sizeof(SEASLOG_ERROR) - 1,    CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_CRITICAL",  SEASLOG_CRITICAL,  sizeof(SEASLOG_CRITICAL) - 1, CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_ALERT",     SEASLOG_ALERT,     sizeof(SEASLOG_ALERT) - 1,    CONST_PERSISTENT | CONST_CS);
    REGISTER_STRINGL_CONSTANT("SEASLOG_EMERGENCY", SEASLOG_EMERGENCY, sizeof(SEASLOG_EMERGENCY) - 1,CONST_PERSISTENT | CONST_CS);

    zend_class_entry seaslog;
    INIT_CLASS_ENTRY(seaslog,SEASLOG_RES_NAME,seaslog_methods);
    seaslog_ce = zend_register_internal_class_ex(&seaslog,NULL,NULL TSRMLS_DC);
    seaslog_ce->ce_flags = ZEND_ACC_IMPLICIT_PUBLIC;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(seaslog)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(seaslog)
{
    seaslog_init_logger(TSRMLS_C);
    seaslog_init_buffer(TSRMLS_C);
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(seaslog)
{
    seaslog_shutdown_buffer(TSRMLS_C);

    return SUCCESS;
}

PHP_MINFO_FUNCTION(seaslog)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "SeasLog support", "Enabled");
    php_info_print_table_row(2, "Version", SEASLOG_VERSION);
    php_info_print_table_row(2, "Author", SEASLOG_AUTHOR);
    php_info_print_table_row(2,"Supports","https://github.com/Neeke/SeasLog");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

void seaslog_init_logger(TSRMLS_D)
{
    SEASLOG_G(base_path) = SEASLOG_G(default_basepath);
    SEASLOG_G(last_logger) = SEASLOG_G(default_logger);
}

void seaslog_init_buffer(TSRMLS_D)
{
    if (SEASLOG_G(use_buffer)) {

        if (SEASLOG_G(SL_globals.log_buffer_counts) == 0) {
            if (SEASLOG_G(SL_globals.log_buffer)) {
                    zval_dtor(SEASLOG_G(SL_globals.log_buffer));
                    FREE_ZVAL(SEASLOG_G(SL_globals.log_buffer));
            }
            MAKE_STD_ZVAL(SEASLOG_G(SL_globals.log_buffer));
            array_init(SEASLOG_G(SL_globals.log_buffer));
            SEASLOG_G(SL_globals.started) = SUCCESS;
            SEASLOG_G(SL_globals.log_buffer_counts) = 0;
        }
    }
}

void seaslog_clear_buffer(TSRMLS_D)
{
    zval_dtor(SEASLOG_G(SL_globals.log_buffer));
    FREE_ZVAL(SEASLOG_G(SL_globals.log_buffer));
    SEASLOG_G(SL_globals.started) = FAILURE;
    SEASLOG_G(SL_globals.log_buffer_counts) = 0;
}

static int real_php_log_ex(char *message, int message_len, char *opt)
{
    php_stream *stream = NULL;
    stream = php_stream_open_wrapper(opt, "a", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!stream) {
        return FAILURE;
    }
    php_stream_write(stream, message, message_len);
    php_stream_close(stream);
    return SUCCESS;
}

static int seaslog_buffer_set(char *log_info,char *path TSRMLS_DC) {
    HashTable   *ht;
    void        *data;
    zval        *log_array;
    zval        *old_log_array;
    zval        *new_log;

    if (!SEASLOG_G(SL_globals.log_buffer) || !(ht = HASH_OF(SEASLOG_G(SL_globals.log_buffer)))) {
        return FAILURE;
    }

    if (zend_hash_find(ht, path, strlen(path) + 1, &data) == SUCCESS) {
        old_log_array = *(zval **) data;

        MAKE_STD_ZVAL(new_log);
        ZVAL_STRING(new_log,log_info,1);

        zend_hash_next_index_insert(Z_ARRVAL_P(old_log_array), (void *)&new_log, sizeof(zval *), NULL);

    } else {
        MAKE_STD_ZVAL(log_array);
        array_init(log_array);

        add_next_index_string(log_array, log_info, 1);
        add_assoc_zval(SEASLOG_G(SL_globals.log_buffer), path, log_array);
    }

    if (SEASLOG_G(buffer_size) > 0) {
        ++SEASLOG_G(SL_globals.log_buffer_counts);

        if (SEASLOG_G(SL_globals.log_buffer_counts) >= SEASLOG_G(buffer_size)) {
            seaslog_shutdown_buffer(TSRMLS_C);
        }
    }

    return SUCCESS;
}

static int seaslog_shutdown_buffer(TSRMLS_D)
{
    if (SEASLOG_G(use_buffer)) {

    if (SEASLOG_G(SL_globals.log_buffer_counts) > 0) {
        HashTable   *ht;
        HashPosition pos;

        if (!SEASLOG_G(SL_globals.log_buffer)) {
            return FAILURE;
        }

        ht = HASH_OF(SEASLOG_G(SL_globals.log_buffer));

        for(zend_hash_internal_pointer_reset_ex(ht, &pos);
            zend_hash_has_more_elements_ex(ht, &pos) == SUCCESS;
            zend_hash_move_forward_ex(ht, &pos))
            {
                char *key;
                uint keylen;
                ulong idx;
                int type;
                int i,num;

                zval **ppzval, tmpcopy;

                type = zend_hash_get_current_key_ex(ht,&key, &keylen,&idx, 0, &pos);
                if (zend_hash_get_current_data_ex(ht,(void**)&ppzval, &pos) == FAILURE) {
                    continue;
                }

                tmpcopy = **ppzval;
                zval_copy_ctor(&tmpcopy);
                INIT_PZVAL(&tmpcopy);

                convert_to_array(&tmpcopy);

                HashTable   *ht_child;
                zval **ppzval_child;
                char *log_info = NULL;
                char *log_info_tmp = NULL;
                char *log_tmp = NULL;

                ht_child = Z_ARRVAL(tmpcopy);
                for(zend_hash_internal_pointer_reset(ht_child);
                        zend_hash_has_more_elements(ht_child) == SUCCESS;
                        zend_hash_move_forward(ht_child))
                {
                    if (zend_hash_get_current_data(ht_child, (void**)&ppzval_child) == FAILURE) {
                        continue;
                    }

                    if (IS_STRING == Z_TYPE_PP(ppzval_child) && Z_STRLEN_PP(ppzval_child)) {
                        log_tmp = Z_STRVAL_PP(ppzval_child);
                        if (log_info) {
                            log_info_tmp = log_info;
                            spprintf(&log_info,0,"%s%s",log_info_tmp,log_tmp);
                        } else {
                            spprintf(&log_info,0,"%s",log_tmp);
                        }

                    }
                }

                int result;
                result = real_php_log_ex(log_info, strlen(log_info),key);

                zval_dtor(&tmpcopy);
            }
        }

        seaslog_clear_buffer(TSRMLS_C);
    }

    return SUCCESS;
}

static char *mk_real_log_path(char *log_path,char *date,char *level TSRMLS_DC){
    char *log_file_path = NULL;
    if (SEASLOG_G(disting_type)){
        spprintf(&log_file_path,0,"%s/%s.%s.log",log_path,level,date);
    }else{
        spprintf(&log_file_path,0,"%s/%s.log",log_path,date);
    }
    
    return log_file_path;
}

static char *delN(char * a){
    int l;
    l=strlen(a);
    a[l-1]='\0';
    return a;
}

static char *mic_time(){
   struct timeval now;
   char *tstr;
   timerclear(&now);
   gettimeofday(&now,NULL);
   spprintf(&tstr,0,"%ld.%ld",(long)time(NULL),(long)now.tv_usec/1000);
   return tstr;
}

static char *mk_real_date(TSRMLS_D){
    char *_date;
    if (SEASLOG_G(disting_by_hour)) {
        _date = php_format_date("YmdH",5,(long)time(NULL),(long)time(NULL));
        } else {
        _date = php_format_date("Ymd",3,(long)time(NULL),(long)time(NULL));
    }

    return _date;
}

static long get_type_count(char *log_path,char *level TSRMLS_DC)
{
    FILE * fp;
    char buffer[BUFSIZ];
    char *str ,*path,*_log_path,*sh;
    long count;

    spprintf(&_log_path, 0, "%s/%s", SEASLOG_G(base_path),SEASLOG_G(last_logger));
    int _ck_dir = _ck_log_dir(_log_path);
    if (_ck_dir == FAILURE){
        return 0;
    }
    
    if (SEASLOG_G(disting_type)){
        spprintf(&path,0,"%s/%s/%s.%s*",SEASLOG_G(base_path),SEASLOG_G(last_logger),level,log_path);
    }else{
        spprintf(&path,0,"%s/%s/%s*",SEASLOG_G(base_path),SEASLOG_G(last_logger),log_path);
    }
    
    spprintf(&sh,0,"more %s | grep '%s' -wc",path,level);
    
    fp = VCWD_POPEN(sh, "r");
    if (!fp){
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to fork [%s]", sh);
        return -1;
    }else{
        char *temp_p = fgets(buffer, sizeof(buffer), fp);
        pclose(fp);
    }

    count = atoi(delN(buffer));
    return count;
}

static int get_detail(char *log_path,char *level,zval *return_value TSRMLS_DC)
{
    FILE * fp;
    char buffer[BUFSIZ+1];
    char *str ,*path ;
    char *sh;
    
    memset(buffer,'\0',sizeof(buffer));

    array_init(return_value);

    if (disting_type){
        spprintf(&path,0,"%s/%s/%s.%s*",SEASLOG_G(base_path),SEASLOG_G(last_logger),level,log_path);
    }else{
        spprintf(&path,0,"%s/%s/%s*",SEASLOG_G(base_path),SEASLOG_G(last_logger),log_path);
    }

    spprintf(&sh,0,"more %s | grep '%s' -w",path,level);

    fp = VCWD_POPEN(sh, "r");

    if (!fp){
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to fork [%s]", sh);
        return -1;
    }else{
       while((fgets(buffer,sizeof(buffer),fp)) != NULL){
            if (strcspn(buffer,SEASLOG_G(base_path)) != 0){
                add_next_index_string(return_value,delN(buffer),1);
            }
       }
       pclose(fp);
    }

    return 1;
}

static void seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAMETERS,char *level)
{
    seaslog_init_buffer(TSRMLS_C);

    int argc = ZEND_NUM_ARGS();
    char *message,*logger = NULL;
    int message_len,logger_len;
    zval **content;

    if (zend_parse_parameters(argc TSRMLS_CC, "s|Zs",&message, &message_len,&content,&logger, &logger_len) == FAILURE)
        return;

    if (argc > 1 && Z_TYPE_PP(content) != IS_ARRAY) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "The second argument is not an array");
        RETURN_FALSE;
    }

    if (argc > 1) {
        if (_seaslog_log_content(argc,level,message,message_len,HASH_OF(*content),logger,logger_len TSRMLS_CC) == FAILURE) {
            RETURN_FALSE;
        }
    } else {
        if (_seaslog_log(argc,level,message,message_len,logger,logger_len TSRMLS_CC) == FAILURE) {
            RETURN_FALSE;
        }
    }

    RETURN_TRUE;
}

PHP_FUNCTION(seaslog_get_version)
{
    char *str;
    int len = 0;
    len = spprintf(&str, 0, "%s", SEASLOG_VERSION);
    RETURN_STRINGL(str, len, 0);
}

PHP_FUNCTION(seaslog_get_author)
{
    char *str;
    int len = 0;
    len = spprintf(&str, 0, "%s", SEASLOG_AUTHOR);
    RETURN_STRINGL(str, len, 0);
}

PHP_METHOD(SEASLOG_RES_NAME,__construct)
{
    seaslog_init_logger(TSRMLS_C);
    seaslog_init_buffer(TSRMLS_C);
}

PHP_METHOD(SEASLOG_RES_NAME,__destruct)
{
    seaslog_shutdown_buffer(TSRMLS_C);
}

PHP_METHOD(SEASLOG_RES_NAME,setBasePath)
{
    char *_base_path = NULL;
    int argc = ZEND_NUM_ARGS();
    int _base_path_len;

    if (zend_parse_parameters(argc TSRMLS_CC, "s", &_base_path, &_base_path_len) == FAILURE)
        return;

    if (argc > 0){
        spprintf(&SEASLOG_G(base_path),0,"%s",_base_path);
        RETURN_TRUE;
    }

    RETURN_FALSE;
}

PHP_METHOD(SEASLOG_RES_NAME,getBasePath)
{
    char *module = NULL;
    int argc = ZEND_NUM_ARGS();
    int module_len;

    if (zend_parse_parameters(argc TSRMLS_CC, "|s", &module, &module_len) == FAILURE)
        return;

    char *str;
    int len = 0;
    len = spprintf(&str, 0, "%s", SEASLOG_G(base_path));
    RETURN_STRINGL(str, len, 0);
}

PHP_METHOD(SEASLOG_RES_NAME,setLogger)
{
    char *module = NULL;
    int argc = ZEND_NUM_ARGS();
    int module_len;

    if (zend_parse_parameters(argc TSRMLS_CC, "s", &module, &module_len) == FAILURE)
        return;

    if (argc > 0){
        spprintf(&SEASLOG_G(last_logger),0,"%s",module);
        RETURN_TRUE;
    }

    RETURN_FALSE;
}

/*the last logger*/
PHP_METHOD(SEASLOG_RES_NAME,getLastLogger)
{
    char *str;
    int len = 0;
    len = spprintf(&str, 0, "%s", SEASLOG_G(last_logger));
    RETURN_STRINGL(str, len, 0);
}


PHP_METHOD(SEASLOG_RES_NAME,analyzerCount)
{
    int argc = ZEND_NUM_ARGS();
    char *log_path,*result,*level;
    int len = 0;
    int log_path_len,level_len;
    long count;

    if (zend_parse_parameters(argc TSRMLS_CC, "|ss",&level,&level_len,&log_path, &log_path_len) == FAILURE)
        return;

    if (argc == 0){
        array_init(return_value);
        log_path = "";

        long count_debug,count_info,count_notice,count_warn,count_erro,count_critical,count_alert,count_emergency;
        count_debug = get_type_count(log_path,SEASLOG_DEBUG);
        count_info = get_type_count(log_path,SEASLOG_INFO);
        count_notice = get_type_count(log_path,SEASLOG_NOTICE);
        count_warn = get_type_count(log_path,SEASLOG_WARNING);
        count_erro = get_type_count(log_path,SEASLOG_ERROR);
        count_critical = get_type_count(log_path,SEASLOG_CRITICAL);
        count_alert = get_type_count(log_path,SEASLOG_ALERT);
        count_emergency = get_type_count(log_path,SEASLOG_EMERGENCY);

        add_assoc_long(return_value, SEASLOG_DEBUG, count_debug);
        add_assoc_long(return_value, SEASLOG_INFO, count_info);
        add_assoc_long(return_value, SEASLOG_NOTICE, count_notice);
        add_assoc_long(return_value, SEASLOG_WARNING, count_warn);
        add_assoc_long(return_value, SEASLOG_ERROR, count_erro);
        add_assoc_long(return_value, SEASLOG_CRITICAL, count_critical);
        add_assoc_long(return_value, SEASLOG_ALERT, count_alert);
        add_assoc_long(return_value, SEASLOG_EMERGENCY, count_emergency);
    }else if (argc == 1){
        log_path = "";
        count = get_type_count(log_path,level);

        RETURN_LONG(count);
    }else{
        count = get_type_count(log_path,level);

        RETURN_LONG(count);
    }

}

PHP_METHOD(SEASLOG_RES_NAME,analyzerDetail)
{
    char *log_path,*level;
    int log_path_len,level_len;
    int argc = ZEND_NUM_ARGS();

    if (zend_parse_parameters(argc TSRMLS_CC, "s|s",&level,&level_len,&log_path, &log_path_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (argc < 2){
        log_path = "*";
    }else{

    }

    get_detail(log_path, level, return_value TSRMLS_CC);
}

PHP_METHOD(SEASLOG_RES_NAME,getBuffer)
{
    if (SEASLOG_G(use_buffer)) {
        RETURN_ZVAL(SEASLOG_G(SL_globals.log_buffer), 1, 0);
    }
}

PHP_METHOD(SEASLOG_RES_NAME,log)
{
    seaslog_init_buffer(TSRMLS_C);

    int argc = ZEND_NUM_ARGS();
    char *message,*logger,*level = NULL;
    int message_len,content_len,level_len,logger_len;
    zval **content;

    if (zend_parse_parameters(argc TSRMLS_CC, "ss|Zs",&level,&level_len,&message, &message_len,&content,&logger, &logger_len) == FAILURE)
        return;

    if (argc > 2 && Z_TYPE_PP(content) != IS_ARRAY) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "The third argument is not an array");
        RETURN_FALSE;
    }

    if (argc > 2) {
        if (_seaslog_log_content(argc,level,message,message_len,HASH_OF(*content),logger,logger_len TSRMLS_CC) == FAILURE) {
            RETURN_FALSE;
        }
    } else {
        if (_seaslog_log(argc,level,message,message_len,logger,logger_len TSRMLS_CC) == FAILURE) {
            RETURN_FALSE;
        }
    }

    RETURN_TRUE;
}

PHP_METHOD(SEASLOG_RES_NAME,debug)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_DEBUG);
}

PHP_METHOD(SEASLOG_RES_NAME,info)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_INFO);
}

PHP_METHOD(SEASLOG_RES_NAME,notice)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_NOTICE);
}

PHP_METHOD(SEASLOG_RES_NAME,warning)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_WARNING);
}

PHP_METHOD(SEASLOG_RES_NAME,error)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_ERROR);
}

PHP_METHOD(SEASLOG_RES_NAME,critical)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_CRITICAL);
}

PHP_METHOD(SEASLOG_RES_NAME,alert)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_ALERT);
}

PHP_METHOD(SEASLOG_RES_NAME,emergency)
{
    seaslog_log_by_level_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, SEASLOG_EMERGENCY);
}

static char *php_strtr_array(char *str, int slen, HashTable *hash)
{
    zval **entry;
    char  *string_key;
    uint   string_key_len;
    zval **trans;
    zval   ctmp;
    ulong num_key;
    int minlen = 128*1024;
    int maxlen = 0, pos, len, found;
    char *key;
    HashPosition hpos;
    smart_str result = {0};
    HashTable tmp_hash;

    zend_hash_init(&tmp_hash, zend_hash_num_elements(hash), NULL, NULL, 0);
    zend_hash_internal_pointer_reset_ex(hash, &hpos);
    while (zend_hash_get_current_data_ex(hash, (void **)&entry, &hpos) == SUCCESS) {
        switch (zend_hash_get_current_key_ex(hash, &string_key, &string_key_len, &num_key, 0, &hpos)) {
        case HASH_KEY_IS_STRING:
            len = string_key_len-1;
            if (len < 1) {
                zend_hash_destroy(&tmp_hash);
            } else {
                zend_hash_add(&tmp_hash, string_key, string_key_len, entry, sizeof(zval*), NULL);
                if (len > maxlen) {
                    maxlen = len;
                }
                if (len < minlen) {
                    minlen = len;
                }
            }
            break;

        case HASH_KEY_IS_LONG:
            Z_TYPE(ctmp) = IS_LONG;
            Z_LVAL(ctmp) = num_key;

            convert_to_string(&ctmp);
            len = Z_STRLEN(ctmp);
            zend_hash_add(&tmp_hash, Z_STRVAL(ctmp), len+1, entry, sizeof(zval*), NULL);
            zval_dtor(&ctmp);

            if (len > maxlen) {
                maxlen = len;
            }
            if (len < minlen) {
                minlen = len;
            }
            break;
        }
        zend_hash_move_forward_ex(hash, &hpos);
    }

    key = emalloc(maxlen+1);
    pos = 0;

    while (pos < slen) {
        if ((pos + maxlen) > slen) {
        maxlen = slen - pos;
        }

        found = 0;
        memcpy(key, str+pos, maxlen);

        for (len = maxlen; len >= minlen; len--) {
            key[len] = 0;

            if (zend_hash_find(&tmp_hash, key, len+1, (void**)&trans) == SUCCESS) {
                char *tval;
                int tlen;
                zval tmp;

                if (Z_TYPE_PP(trans) != IS_STRING) {
                    tmp = **trans;
                    zval_copy_ctor(&tmp);
                    convert_to_string(&tmp);
                    tval = Z_STRVAL(tmp);
                    tlen = Z_STRLEN(tmp);
                } else {
                    tval = Z_STRVAL_PP(trans);
                    tlen = Z_STRLEN_PP(trans);
                }

                smart_str_appendl(&result, tval, tlen);
                pos += len;
                found = 1;

                if (Z_TYPE_PP(trans) != IS_STRING) {
                    zval_dtor(&tmp);
                }
                break;
            }
        }

        if (! found) {
            smart_str_appendc(&result, str[pos++]);
        }
    }

    efree(key);
    zend_hash_destroy(&tmp_hash);
    smart_str_0(&result);
    return result.c;
}

PHPAPI int _seaslog_log_content(int argc,char *level,char *message,int message_len,HashTable *content,char *module,int module_len TSRMLS_DC)
{
    char *result = php_strtr_array(message,message_len,content);

    return _seaslog_log(argc,level,result,strlen(result),module,module_len TSRMLS_CC);
}

PHPAPI int _seaslog_log(int argc,char *level,char *message,int message_len,char *module,int module_len TSRMLS_DC)
{
    char *logger,*_log_path,*_date ,*_time,*log_file_path,*log_info;
    int log_len;

    if (argc > 2){
        logger = module;
        if (strcmp(SEASLOG_G(last_logger),"default") == 0){
            spprintf(&SEASLOG_G(last_logger),0,"%s",logger);
        }
    } else {
        logger = SEASLOG_G(last_logger);
    }

    if (_check_level(level TSRMLS_CC) == FAILURE) {
      return FAILURE;
    }
    
    spprintf(&_log_path, 0, "%s/%s", SEASLOG_G(base_path),logger);
    _mk_log_dir(_log_path);

    _date = mk_real_date();

    _time = php_format_date("Y:m:d H:i:s",11,(long)time(NULL),(long)time(NULL));

    log_file_path = mk_real_log_path(_log_path,_date,level TSRMLS_CC);

    log_len = spprintf(&log_info, 0, "%s | %d | %s | %s | %s \n",level,getpid(), mic_time(),_time, message);

    if (_php_log_ex(log_info, log_len, log_file_path TSRMLS_CC) == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

/*check the level*/
PHPAPI int _check_level(char *level TSRMLS_DC)
{
  if (SEASLOG_G(level) < 1) return SUCCESS;
  if (SEASLOG_G(level) > 8) return FAILURE;
  
  if (strcmp(level,SEASLOG_DEBUG) == 0 && SEASLOG_G(level) <= 1) return SUCCESS;
  if (strcmp(level,SEASLOG_INFO) == 0 && SEASLOG_G(level) <= 2) return SUCCESS;
  if (strcmp(level,SEASLOG_NOTICE) == 0 && SEASLOG_G(level) <= 3) return SUCCESS;
  if (strcmp(level,SEASLOG_WARNING) == 0 && SEASLOG_G(level) <= 4) return SUCCESS;
  if (strcmp(level,SEASLOG_ERROR) == 0 && SEASLOG_G(level) <= 5) return SUCCESS;
  if (strcmp(level,SEASLOG_CRITICAL) == 0 && SEASLOG_G(level) <= 6) return SUCCESS;
  if (strcmp(level,SEASLOG_ALERT) == 0 && SEASLOG_G(level) <= 7) return SUCCESS;
  if (strcmp(level,SEASLOG_EMERGENCY) == 0 && SEASLOG_G(level) <= 8) return SUCCESS;

  return FAILURE;
}

PHPAPI int _mk_log_dir(char *dir)
{
    int _ck_dir = _ck_log_dir(dir);

    if (_ck_dir == FAILURE){
        zval *zcontext = NULL;
        long mode = 0777;
        zend_bool recursive = 1;
        php_stream_context *context;

        context = php_stream_context_from_zval(zcontext, 0);

        if (php_stream_mkdir(dir, mode, (recursive ? PHP_STREAM_MKDIR_RECURSIVE : 0) | REPORT_ERRORS, context)){
            return FAILURE;
        }

        return SUCCESS;
    }else {
        return FAILURE;
    }

}

PHPAPI int _ck_log_dir(char *dir)
{
    zval *function_name;
    zval *retval;
    zval *str;
    zval **param[1];
    MAKE_STD_ZVAL(function_name);
    ZVAL_STRING(function_name , "is_dir", 1);

    MAKE_STD_ZVAL(str);
    ZVAL_STRING(str, dir, 1);
    param[0] = &str;

    if(call_user_function_ex(EG(function_table), NULL, function_name, &retval, 1, param,0, EG(active_symbol_table)) != SUCCESS)
    {
        zend_error(E_ERROR, "Function call failed");
    }

    if (retval != NULL && zval_is_true(retval)){
        return SUCCESS;
    }

    return FAILURE;
}

PHPAPI int _php_log_ex(char *message, int message_len, char *opt TSRMLS_DC)
{
    if (SEASLOG_G(use_buffer)) {
        seaslog_buffer_set(message,opt TSRMLS_CC);
        return SUCCESS;
    } else {
        return real_php_log_ex(message, message_len, opt);
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
