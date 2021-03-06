/*
    Ophidia Analytics Framework
    Copyright (C) 2012-2017 CMCC Foundation

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#include <errmsg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include <glob.h>
#include <dirent.h>
#include <sys/stat.h>

#include "drivers/OPH_FS_operator.h"
#include "oph_analytics_operator_library.h"

#include "oph_framework_paths.h"

#include "oph_idstring_library.h"
#include "oph_task_parser_library.h"
#include "oph_pid_library.h"
#include "oph_json_library.h"
#include "ophidiadb/oph_odb_filesystem_library.h"

#include "debug.h"

#include "oph_input_parameters.h"
#include "oph_log_error_codes.h"

#define OPH_FS_HPREFIX '.'
#define OPH_SEPARATOR_PARAM ";"

int cmpfunc(const void *a, const void *b)
{
	char const *aa = (char const *) a;
	char const *bb = (char const *) b;
	return strcmp(aa, bb);
}

int openDir(const char *path, int recursive, size_t * counter, char **buffer, char *file)
{
	if (!path || !counter || !buffer)
		return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;

	DIR *dirp = opendir(path);
	if (!dirp)
		return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;

	struct dirent *entry = NULL, save_entry;
	char *sub;
	int s;

	if (recursive < 0)
		recursive++;

	if (file) {
		glob_t globbuf;
		char *path_and_file = NULL;
		s = asprintf(&path_and_file, "%s/%s", path, file);
		if (!path_and_file) {
			closedir(dirp);
			return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
		}
		if ((s = glob(path_and_file, GLOB_MARK | GLOB_NOSORT, NULL, &globbuf))) {
			if (s != GLOB_NOMATCH) {
				pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to parse '%s'\n", path_and_file);
				free(path_and_file);
				closedir(dirp);
				return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
			} else {
				pmesg(LOG_WARNING, __FILE__, __LINE__, "No object found.\n");
				if (!recursive) {
					free(path_and_file);
					closedir(dirp);
					return OPH_ANALYTICS_OPERATOR_SUCCESS;
				}
			}
		}
		free(path_and_file);
		*counter += globbuf.gl_pathc;
		size_t i;
		for (i = 0; i < globbuf.gl_pathc; ++i) {
			if (*buffer) {
				sub = *buffer;
				s = asprintf(buffer, "%s%s%s", sub, globbuf.gl_pathv[i], OPH_SEPARATOR_PARAM);
				free(sub);
			} else
				s = asprintf(buffer, "%s%s", globbuf.gl_pathv[i], OPH_SEPARATOR_PARAM);
		}
		globfree(&globbuf);
	}

	char full_filename[PATH_MAX];
	struct stat file_stat;

	int result;
	while (!readdir_r(dirp, &save_entry, &entry) && entry) {
		if (*entry->d_name != OPH_FS_HPREFIX) {
			snprintf(full_filename, PATH_MAX, "%s/%s", path, entry->d_name);
			lstat(full_filename, &file_stat);
			if (!file && (S_ISREG(file_stat.st_mode) || S_ISLNK(file_stat.st_mode) || S_ISDIR(file_stat.st_mode))) {
				(*counter)++;
				if (*buffer) {
					sub = *buffer;
					s = asprintf(buffer, "%s%s/%s%s", sub, path, entry->d_name, OPH_SEPARATOR_PARAM);
					free(sub);
				} else
					s = asprintf(buffer, "%s/%s%s", path, entry->d_name, OPH_SEPARATOR_PARAM);
			}
			if (recursive && S_ISDIR(file_stat.st_mode)) {
				sub = NULL;
				s = asprintf(&sub, "%s/%s", path, entry->d_name);
				if (!sub)
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
				else {
					result = openDir(sub, recursive, counter, buffer, file);
					free(sub);
				}
				if (result) {
					closedir(dirp);
					return result;
				}
			}
		}
	}
	closedir(dirp);

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int write_json(char (*filenames)[OPH_COMMON_BUFFER_LEN], int jj, oph_json * operator_json, int num_fields)
{
	int ii, iii, jjj, result = OPH_ANALYTICS_OPERATOR_SUCCESS;
	char **jsonvalues = NULL;

	qsort(filenames, jj, OPH_COMMON_BUFFER_LEN, cmpfunc);

	for (ii = 0; ii < jj; ++ii) {
		jsonvalues = (char **) calloc(num_fields, sizeof(char *));
		if (!jsonvalues) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "values");
			result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
			break;
		}
		jjj = 0;
		jsonvalues[jjj] = strdup(filenames[ii][strlen(filenames[ii]) - 1] == '/' ? "d" : "f");
		if (!jsonvalues[jjj]) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_EXPLORECUBE_MEMORY_ERROR_INPUT, "value");
			for (iii = 0; iii < jjj; iii++)
				if (jsonvalues[iii])
					free(jsonvalues[iii]);
			if (jsonvalues)
				free(jsonvalues);
			result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
			break;
		}
		jjj++;
		jsonvalues[jjj] = strdup(filenames[ii]);
		if (!jsonvalues[jjj]) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_EXPLORECUBE_MEMORY_ERROR_INPUT, "value");
			for (iii = 0; iii < jjj; iii++)
				if (jsonvalues[iii])
					free(jsonvalues[iii]);
			if (jsonvalues)
				free(jsonvalues);
			result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
			break;
		}
		if (oph_json_add_grid_row(operator_json, OPH_JSON_OBJKEY_FS, jsonvalues)) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID ROW error\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID ROW error\n");
			for (iii = 0; iii < num_fields; iii++)
				if (jsonvalues[iii])
					free(jsonvalues[iii]);
			if (jsonvalues)
				free(jsonvalues);
			result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
			break;
		}
		for (iii = 0; iii < num_fields; iii++)
			if (jsonvalues[iii])
				free(jsonvalues[iii]);
		if (jsonvalues)
			free(jsonvalues);
	}

	return result;
}

int env_set(HASHTBL * task_tbl, oph_operator_struct * handle)
{
	if (!handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	if (!task_tbl) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null operator string\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_TASK_TABLE);
		return OPH_ANALYTICS_OPERATOR_BAD_PARAMETER;
	}

	if (handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator handle already initialized\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_HANDLE_ALREADY_INITIALIZED);
		return OPH_ANALYTICS_OPERATOR_NOT_NULL_OPERATOR_HANDLE;
	}

	if (!(handle->operator_handle = (OPH_FS_operator_handle *) calloc(1, sizeof(OPH_FS_operator_handle)))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
	}
	//1 - Set up struct to empty values
	((OPH_FS_operator_handle *) handle->operator_handle)->cwd = NULL;
	((OPH_FS_operator_handle *) handle->operator_handle)->user = NULL;
	((OPH_FS_operator_handle *) handle->operator_handle)->mode = -1;
	((OPH_FS_operator_handle *) handle->operator_handle)->path = NULL;
	((OPH_FS_operator_handle *) handle->operator_handle)->file = NULL;
	((OPH_FS_operator_handle *) handle->operator_handle)->recursive = 0;
	((OPH_FS_operator_handle *) handle->operator_handle)->depth = 0;
	((OPH_FS_operator_handle *) handle->operator_handle)->realpath = 0;
	((OPH_FS_operator_handle *) handle->operator_handle)->objkeys = NULL;
	((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num = -1;

	//Only master process has to continue
	if (handle->proc_rank != 0)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	char *value;

	// retrieve objkeys
	value = hashtbl_get(task_tbl, OPH_IN_PARAM_OBJKEY_FILTER);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_OBJKEY_FILTER);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_FRAMEWORK_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_OBJKEY_FILTER);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (oph_tp_parse_multiple_value_param(value, &((OPH_FS_operator_handle *) handle->operator_handle)->objkeys, &((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator string not valid\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Operator string not valid\n");
		oph_tp_free_multiple_value_param_list(((OPH_FS_operator_handle *) handle->operator_handle)->objkeys, ((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_SYSTEM_COMMAND);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_SYSTEM_COMMAND);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_SYSTEM_COMMAND);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!strcasecmp(value, OPH_FS_CMD_CD)) {
		((OPH_FS_operator_handle *) handle->operator_handle)->mode = OPH_FS_MODE_CD;
	} else if (!strcasecmp(value, OPH_FS_CMD_LS)) {
		((OPH_FS_operator_handle *) handle->operator_handle)->mode = OPH_FS_MODE_LS;
	} else {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Invalid input parameter %s\n", value);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_PARAMETER, value);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_DATA_PATH);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_DATA_PATH);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_DATA_PATH);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!(((OPH_FS_operator_handle *) handle->operator_handle)->path = (char *) strdup(value))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_STRING);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_FILE);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_FILE);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_FILE);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!(((OPH_FS_operator_handle *) handle->operator_handle)->file = (char *) strdup(value))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_STRING);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_CDD);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_CDD);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_CDD);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!(((OPH_FS_operator_handle *) handle->operator_handle)->cwd = (char *) strdup(value))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, OPH_IN_PARAM_CDD);
		return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_RECURSIVE_SEARCH);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_RECURSIVE_SEARCH);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_RECURSIVE_SEARCH);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!strcmp(value, OPH_COMMON_YES_VALUE)) {
		((OPH_FS_operator_handle *) handle->operator_handle)->recursive = 1;
	} else if (strcmp(value, OPH_COMMON_NO_VALUE)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator string not valid\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_STRING);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_DEPTH);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_DEPTH);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_DEPTH);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	((OPH_FS_operator_handle *) handle->operator_handle)->depth = (int) strtol(value, NULL, 10);
	if (((OPH_FS_operator_handle *) handle->operator_handle)->depth < 0) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Invalid input parameter %s\n", value);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_PARAMETER, value);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_REALPATH);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_IN_PARAM_REALPATH);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_REALPATH);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!strcmp(value, OPH_COMMON_YES_VALUE)) {
		((OPH_FS_operator_handle *) handle->operator_handle)->realpath = 1;
	} else if (strcmp(value, OPH_COMMON_NO_VALUE)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator string not valid\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_STRING);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_ARG_USERNAME);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter '%s'\n", OPH_ARG_USERNAME);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MISSING_INPUT_PARAMETER, OPH_ARG_USERNAME);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (!(((OPH_FS_operator_handle *) handle->operator_handle)->user = (char *) strdup(value))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, OPH_IN_PARAM_CDD);
		return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_init(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_distribute(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_execute(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}
	//Only master process has to continue
	if (handle->proc_rank != 0)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	char *abs_path = NULL;
	if (oph_pid_get_base_src_path(&abs_path)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to read base src_path\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Unable to read base src_path\n");
		return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
	}
	if (!abs_path)
		abs_path = strdup("/");

	char *rel_path = NULL;
	char is_valid = strcasecmp(((OPH_FS_operator_handle *) handle->operator_handle)->path, OPH_FRAMEWORK_FS_DEFAULT_PATH);
	char file_is_valid = strcasecmp(((OPH_FS_operator_handle *) handle->operator_handle)->file, OPH_FRAMEWORK_FS_DEFAULT_PATH);
	if (oph_odb_fs_path_parsing(is_valid ? ((OPH_FS_operator_handle *) handle->operator_handle)->path : "", ((OPH_FS_operator_handle *) handle->operator_handle)->cwd, NULL, &rel_path, NULL)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to parse path\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_PATH_PARSING_ERROR);
		if (abs_path) {
			free(abs_path);
			abs_path = NULL;
		}
		return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
	}
	size_t len = strlen(rel_path);
	if (len > 1)
		rel_path[len - 1] = 0;

	char path[OPH_COMMON_BUFFER_LEN];
	snprintf(path, OPH_COMMON_BUFFER_LEN, "%s%s", abs_path, rel_path);
	printf(OPH_FS_CD_MESSAGE " is: %s from %s and %s\n", path, ((OPH_FS_operator_handle *) handle->operator_handle)->path, ((OPH_FS_operator_handle *) handle->operator_handle)->cwd);
	int result = OPH_ANALYTICS_OPERATOR_SUCCESS;
	struct stat file_stat;

	switch (((OPH_FS_operator_handle *) handle->operator_handle)->mode) {

		case OPH_FS_MODE_CD:

			if (stat(path, &file_stat) || !S_ISDIR(file_stat.st_mode)) {
				pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to access '%s'\n", rel_path);
				logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Unable to access '%s'\n", rel_path);
				result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
				break;
			}
			// ADD OUTPUT TO JSON AS TEXT
			if (oph_json_is_objkey_printable
			    (((OPH_FS_operator_handle *) handle->operator_handle)->objkeys, ((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num, OPH_JSON_OBJKEY_FS)) {
				if (oph_json_add_text(handle->operator_json, OPH_JSON_OBJKEY_FS, OPH_FS_CD_MESSAGE, rel_path)) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD TEXT error\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD TEXT error\n");
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
			}
			// ADD OUTPUT CWD TO NOTIFICATION STRING
			char tmp_string[OPH_COMMON_BUFFER_LEN];
			snprintf(tmp_string, OPH_COMMON_BUFFER_LEN, "%s=%s;", OPH_IN_PARAM_CDD, rel_path);
			if (handle->output_string) {
				strncat(tmp_string, handle->output_string, OPH_COMMON_BUFFER_LEN - strlen(tmp_string));
				free(handle->output_string);
			}
			handle->output_string = strdup(tmp_string);

			break;

		case OPH_FS_MODE_LS:

			// ADD OUTPUT TO JSON AS TEXT
			if (oph_json_is_objkey_printable
			    (((OPH_FS_operator_handle *) handle->operator_handle)->objkeys, ((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num, OPH_JSON_OBJKEY_FS)) {

				size_t ii = 0, jj = 0;
				int num_fields = 2, iii, jjj = 0;

				// Header
				char **jsonkeys = (char **) malloc(sizeof(char *) * num_fields);
				if (!jsonkeys) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "keys");
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
				jsonkeys[jjj] = strdup("T");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
				jjj++;
				jsonkeys[jjj] = strdup("OBJECT");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
				jjj = 0;
				char **fieldtypes = (char **) malloc(sizeof(char *) * num_fields);
				if (!fieldtypes) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "fieldtypes");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "fieldtype");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					for (iii = 0; iii < jjj; iii++)
						if (fieldtypes[iii])
							free(fieldtypes[iii]);
					if (fieldtypes)
						free(fieldtypes);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_MEMORY_ERROR_INPUT, "fieldtype");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					for (iii = 0; iii < jjj; iii++)
						if (fieldtypes[iii])
							free(fieldtypes[iii]);
					if (fieldtypes)
						free(fieldtypes);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}

				if (oph_json_add_grid(handle->operator_json, OPH_JSON_OBJKEY_FS, rel_path, NULL, jsonkeys, num_fields, fieldtypes, num_fields)) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID error\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID error\n");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					for (iii = 0; iii < num_fields; iii++)
						if (fieldtypes[iii])
							free(fieldtypes[iii]);
					if (fieldtypes)
						free(fieldtypes);
					result = OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					break;
				}

				for (iii = 0; iii < num_fields; iii++)
					if (jsonkeys[iii])
						free(jsonkeys[iii]);
				if (jsonkeys)
					free(jsonkeys);
				for (iii = 0; iii < num_fields; iii++)
					if (fieldtypes[iii])
						free(fieldtypes[iii]);
				if (fieldtypes)
					free(fieldtypes);

				// Data

				int recursive = 0;
				if (((OPH_FS_operator_handle *) handle->operator_handle)->recursive) {
					recursive = 1;
					if (((OPH_FS_operator_handle *) handle->operator_handle)->depth)
						recursive = -((OPH_FS_operator_handle *) handle->operator_handle)->depth;
				}

				char real_path[PATH_MAX], *_real_path;
				if (strchr(path, '*') || strchr(path, '~') || strchr(path, '{') || strchr(path, '}'))	// Use glob
				{
					if (recursive) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Recursive option cannot be selected for '%s'\n", ((OPH_FS_operator_handle *) handle->operator_handle)->path);
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Recursive option cannot be selected for '%s'\n",
							((OPH_FS_operator_handle *) handle->operator_handle)->path);
						result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
						break;
					}
					int s;
					glob_t globbuf;
					if ((s = glob(path, GLOB_MARK | GLOB_NOSORT | GLOB_TILDE_CHECK | GLOB_BRACE, NULL, &globbuf))) {
						if (s != GLOB_NOMATCH) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to parse '%s'\n", ((OPH_FS_operator_handle *) handle->operator_handle)->path);
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Unable to parse '%s'\n",
								((OPH_FS_operator_handle *) handle->operator_handle)->path);
							result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
						}
						break;
					}
					for (ii = 0; ii < globbuf.gl_pathc; ++ii) {
						if (!realpath(globbuf.gl_pathv[ii], real_path)) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Wrong path name '%s'\n", globbuf.gl_pathv[ii]);
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Wrong path name '%s'\n", globbuf.gl_pathv[ii]);
							result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
							break;
						}
						lstat(real_path, &file_stat);
						if (S_ISREG(file_stat.st_mode) || S_ISLNK(file_stat.st_mode) || S_ISDIR(file_stat.st_mode))
							jj++;
					}
					if (ii < globbuf.gl_pathc) {
						globfree(&globbuf);
						break;
					}
					char filenames[jj][OPH_COMMON_BUFFER_LEN];
					for (ii = jj = 0; ii < globbuf.gl_pathc; ++ii) {
						if (!realpath(globbuf.gl_pathv[ii], real_path))
							break;
						lstat(real_path, &file_stat);
						if (S_ISREG(file_stat.st_mode) || S_ISLNK(file_stat.st_mode) || S_ISDIR(file_stat.st_mode)) {
							if (((OPH_FS_operator_handle *) handle->operator_handle)->realpath) {
								_real_path = real_path;
								if (strlen(abs_path) > 1)
									for (iii = 0; _real_path && *_real_path && (*_real_path == abs_path[iii]); iii++, _real_path++);
							} else
								_real_path = strrchr(real_path, '/') + 1;
							if (!_real_path || !*_real_path) {
								pmesg(LOG_ERROR, __FILE__, __LINE__, "Error in handling real path of '%s'\n", globbuf.gl_pathv[ii]);
								logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Error in handling real path of '%s'\n", globbuf.gl_pathv[ii]);
								result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
								break;
							}
							snprintf(filenames[jj++], OPH_COMMON_BUFFER_LEN, "%s%s", _real_path, S_ISDIR(file_stat.st_mode) ? "/" : "");
						}
					}
					globfree(&globbuf);
					if (ii < globbuf.gl_pathc)
						break;

					result = write_json(filenames, jj, handle->operator_json, num_fields);

				} else {

					if (!realpath(path, real_path)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Wrong path name '%s'\n", path);
						result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
						break;
					}

					char *tbuffer = NULL;
					if (openDir(real_path, recursive, &jj, &tbuffer, file_is_valid ? ((OPH_FS_operator_handle *) handle->operator_handle)->file : NULL)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to open '%s'.\n", path);
						result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
						if (tbuffer)
							free(tbuffer);
						break;
					}

					if (jj && tbuffer) {

						unsigned int nn = strlen(abs_path) > 1, ni;
						char *filename = real_path;
						while ((filename = strchr(filename, '/'))) {
							nn++;
							filename++;
						}

						char filenames[jj][OPH_COMMON_BUFFER_LEN], *start = tbuffer, *save_pointer = NULL;
						for (ii = 0; ii < jj; ++ii, start = NULL) {
							filename = strtok_r(start, OPH_SEPARATOR_PARAM, &save_pointer);
							lstat(filename, &file_stat);
							for (ni = 0; ni < nn; ++ni)
								filename = strchr(filename, '/') + 1;
							snprintf(filenames[ii], OPH_COMMON_BUFFER_LEN, "%s%s", filename, S_ISDIR(file_stat.st_mode) ? "/" : "");
						}
						result = write_json(filenames, jj, handle->operator_json, num_fields);
					}
					if (tbuffer)
						free(tbuffer);
				}
			}

			break;

		default:
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Invalid input parameter %s\n", OPH_IN_PARAM_SYSTEM_COMMAND);
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_INVALID_INPUT_PARAMETER, OPH_IN_PARAM_SYSTEM_COMMAND);
			result = OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	if (abs_path) {
		free(abs_path);
		abs_path = NULL;
	}
	if (rel_path) {
		free(rel_path);
		rel_path = NULL;
	}

	return result;
}

int task_reduce(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_destroy(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_FS_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int env_unset(oph_operator_struct * handle)
{
	//If NULL return success; it's already free
	if (!handle || !handle->operator_handle)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	if (((OPH_FS_operator_handle *) handle->operator_handle)->path) {
		free((char *) ((OPH_FS_operator_handle *) handle->operator_handle)->path);
		((OPH_FS_operator_handle *) handle->operator_handle)->path = NULL;
	}
	if (((OPH_FS_operator_handle *) handle->operator_handle)->file) {
		free((char *) ((OPH_FS_operator_handle *) handle->operator_handle)->file);
		((OPH_FS_operator_handle *) handle->operator_handle)->file = NULL;
	}
	if (((OPH_FS_operator_handle *) handle->operator_handle)->cwd) {
		free((char *) ((OPH_FS_operator_handle *) handle->operator_handle)->cwd);
		((OPH_FS_operator_handle *) handle->operator_handle)->cwd = NULL;
	}
	if (((OPH_FS_operator_handle *) handle->operator_handle)->user) {
		free((char *) ((OPH_FS_operator_handle *) handle->operator_handle)->user);
		((OPH_FS_operator_handle *) handle->operator_handle)->user = NULL;
	}
	if (((OPH_FS_operator_handle *) handle->operator_handle)->objkeys) {
		oph_tp_free_multiple_value_param_list(((OPH_FS_operator_handle *) handle->operator_handle)->objkeys, ((OPH_FS_operator_handle *) handle->operator_handle)->objkeys_num);
		((OPH_FS_operator_handle *) handle->operator_handle)->objkeys = NULL;
	}
	free((OPH_FS_operator_handle *) handle->operator_handle);
	handle->operator_handle = NULL;

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}
