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

#include "drivers/OPH_INSTANCES_operator.h"
#define _GNU_SOURCE
#include <errmsg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "oph_analytics_operator_library.h"

#include "oph_idstring_library.h"
#include "oph_task_parser_library.h"
#include "oph_json_library.h"

#include "debug.h"

#include "oph_input_parameters.h"
#include "oph_log_error_codes.h"

int env_set(HASHTBL * task_tbl, oph_operator_struct * handle)
{
	if (!handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	if (!task_tbl) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null operator string\n");
		return OPH_ANALYTICS_OPERATOR_BAD_PARAMETER;
	}

	if (handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator handle already initialized\n");
		return OPH_ANALYTICS_OPERATOR_NOT_NULL_OPERATOR_HANDLE;
	}

	if (!(handle->operator_handle = (OPH_INSTANCES_operator_handle *) calloc(1, sizeof(OPH_INSTANCES_operator_handle)))) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
	}
	//1 - Set up struct to empty values
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level = 0;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->fs_type = 0;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys = NULL;
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys_num = -1;

	ophidiadb *oDB = &((OPH_INSTANCES_operator_handle *) handle->operator_handle)->oDB;

	//Only master process has to continue
	if (handle->proc_rank != 0)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	oph_odb_init_ophidiadb(oDB);


	if (oph_odb_read_ophidiadb_config_file(oDB)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to read OphidiaDB configuration\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_OPHIDIADB_CONFIGURATION_FILE_NO_CONTAINER);

		return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
	}
	//3 - Fill struct with the correct data
	char *value;

	// retrieve objkeys
	value = hashtbl_get(task_tbl, OPH_IN_PARAM_OBJKEY_FILTER);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_OBJKEY_FILTER);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_FRAMEWORK_MISSING_INPUT_PARAMETER, OPH_IN_PARAM_OBJKEY_FILTER);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (oph_tp_parse_multiple_value_param(value, &((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys, &((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys_num)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Operator string not valid\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "Operator string not valid\n");
		oph_tp_free_multiple_value_param_list(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys, ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys_num);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_VISUALIZZATION_LEVEL);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_VISUALIZZATION_LEVEL);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_VISUALIZZATION_LEVEL);

		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level = (int) strtol(value, NULL, 10);
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level < 1 || ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level > 3) {
		pmesg(LOG_WARNING, __FILE__, __LINE__, "INSTANCES level unrecognized\n");
		logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_BAD_LEVEL_PARAMETER, ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level, 1);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level = 1;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_HOSTNAME_FILTER);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_HOSTNAME_FILTER);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_HOSTNAME_FILTER);

		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_ALL_FILTER) != 0) {
		if (!(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname = (char *) strndup(value, OPH_TP_TASKLEN))) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "hostname");

			return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
		}
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_PARTITION_NAME);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_PARTITION_NAME);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_PARTITION_NAME);

		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_ALL_FILTER) != 0) {
		if (!(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name = (char *) strndup(value, OPH_TP_TASKLEN))) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "partition_name");

			return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
		}
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_FS_TYPE_FILTER);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_FS_TYPE_FILTER);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_FS_TYPE_FILTER);

		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_IO_FS_LOCAL) == 0) {
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->fs_type = OPH_COMMON_IO_FS_LOCAL_TYPE;
	} else if (strcmp(value, OPH_COMMON_IO_FS_GLOBAL) == 0) {
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->fs_type = OPH_COMMON_IO_FS_GLOBAL_TYPE;
	} else {
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->fs_type = OPH_INSTANCES_ALL_FS_TYPE;
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_IOSERVER_TYPE_FILTER);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_IOSERVER_TYPE_FILTER);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_IOSERVER_TYPE_FILTER);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_ALL_FILTER) != 0) {
		if (!(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type = (char *) strndup(value, OPH_TP_TASKLEN))) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "I/O server type");
			return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
		}
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_HOST_STATUS);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_HOST_STATUS);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_HOST_STATUS);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_UP_STATUS) == 0 || strcmp(value, OPH_COMMON_DOWN_STATUS) == 0) {
		if (!(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status = (char *) strndup(value, OPH_TP_TASKLEN))) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "host_status");
			return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
		}
	}

	value = hashtbl_get(task_tbl, OPH_IN_PARAM_DBMS_STATUS);
	if (!value) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Missing input parameter %s\n", OPH_IN_PARAM_DBMS_STATUS);
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MISSING_INPUT_PARAMETER, "NO-CONTAINER", OPH_IN_PARAM_DBMS_STATUS);
		return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}
	if (strcmp(value, OPH_COMMON_UP_STATUS) == 0 || strcmp(value, OPH_COMMON_DOWN_STATUS) == 0) {
		if (!(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status = (char *) strndup(value, OPH_TP_TASKLEN))) {
			pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
			logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "dbms_status");
			return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
		}
	}

	if (oph_odb_connect_to_ophidiadb(oDB)) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Unable to connect to OphidiaDB. Check access parameters.\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_OPHIDIADB_CONNECTION_ERROR_NO_CONTAINER);
		return OPH_ANALYTICS_OPERATOR_MYSQL_ERROR;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_init(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_distribute(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_execute(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}
	//Only master process has to continue
	if (handle->proc_rank != 0)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	ophidiadb *oDB = &((OPH_INSTANCES_operator_handle *) handle->operator_handle)->oDB;
	int level = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->level;
	char *hostname = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname;
	char *partition_name = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name;
	int fs_type = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->fs_type;
	char *ioserver_type = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type;
	char *host_status = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status;
	char *dbms_status = ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status;

	MYSQL_RES *info_instances = NULL;
	int num_rows = 0;

	int objkey_printable =
	    oph_json_is_objkey_printable(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys, ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys_num,
					 OPH_JSON_OBJKEY_INSTANCES);

	switch (level) {
		case 3:
			if (partition_name) {
				printf("+----------------------+----------------------+-------------+\n");
				printf("| %-20s | %-20s | %-11s |\n", "HOST PARTITION", "HOST NAME", "HOST STATUS");
				printf("+----------------------+----------------------+-------------+\n");

				// SET TABLE COLUMN FOR JSON
				if (objkey_printable) {
					char **jsonkeys = NULL;
					char **fieldtypes = NULL;
					int num_fields = 3, iii, jjj = 0;
					jsonkeys = (char **) malloc(sizeof(char *) * num_fields);
					if (!jsonkeys) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "keys");
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jsonkeys[jjj] = strdup("HOST PARTITION");
					if (!jsonkeys[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
						for (iii = 0; iii < jjj; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj++;
					jsonkeys[jjj] = strdup("HOST NAME");
					if (!jsonkeys[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
						for (iii = 0; iii < jjj; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj++;
					jsonkeys[jjj] = strdup("HOST STATUS");
					if (!jsonkeys[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
						for (iii = 0; iii < jjj; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj = 0;
					fieldtypes = (char **) malloc(sizeof(char *) * num_fields);
					if (!fieldtypes) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtypes");
						for (iii = 0; iii < num_fields; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					fieldtypes[jjj] = strdup(OPH_JSON_STRING);
					if (!fieldtypes[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj++;
					fieldtypes[jjj] = strdup(OPH_JSON_STRING);
					if (!fieldtypes[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj++;
					fieldtypes[jjj] = strdup(OPH_JSON_STRING);
					if (!fieldtypes[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					if (oph_json_add_grid(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, "Host Partition : Host List", NULL, jsonkeys, num_fields, fieldtypes, num_fields)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID error\n");
						logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID error\n");
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
						return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
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
				}

			} else {
				printf("+----------------------+\n");
				printf("| %-20s |\n", "HOST PARTITION");
				printf("+----------------------+\n");

				// SET TABLE COLUMN FOR JSON
				if (objkey_printable) {
					char **jsonkeys = NULL;
					char **fieldtypes = NULL;
					int num_fields = 1, iii, jjj = 0;
					jsonkeys = (char **) malloc(sizeof(char *) * num_fields);
					if (!jsonkeys) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "keys");
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jsonkeys[jjj] = strdup("HOST PARTITION");
					if (!jsonkeys[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
						for (iii = 0; iii < jjj; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					jjj = 0;
					fieldtypes = (char **) malloc(sizeof(char *) * num_fields);
					if (!fieldtypes) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtypes");
						for (iii = 0; iii < num_fields; iii++)
							if (jsonkeys[iii])
								free(jsonkeys[iii]);
						if (jsonkeys)
							free(jsonkeys);
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					fieldtypes[jjj] = strdup(OPH_JSON_STRING);
					if (!fieldtypes[jjj]) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					if (oph_json_add_grid(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, "Host Partition List", NULL, jsonkeys, num_fields, fieldtypes, num_fields)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID error\n");
						logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID error\n");
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
						return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
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
				}

			}
			break;
		case 2:
			printf("+----------------------+-------------+----------------+----------+-----------------+----------------------+-----------------+\n");
			printf("| %-20s | %-11s | %-14s | %-8s | %-15s | %-20s | %-15s |\n", "HOSTNAME", "HOST STATUS", "DBMS INSTANCE", "PORT", "FILESYSTEM TYPE", "I/O SERVER TYPE",
			       "INSTANCE STATUS");
			printf("+----------------------+-------------+----------------+----------+-----------------+----------------------+-----------------+\n");

			// SET TABLE COLUMN FOR JSON
			if (objkey_printable) {
				char **jsonkeys = NULL;
				char **fieldtypes = NULL;
				int num_fields = 7, iii, jjj = 0;
				jsonkeys = (char **) malloc(sizeof(char *) * num_fields);
				if (!jsonkeys) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "keys");
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jsonkeys[jjj] = strdup("HOST NAME");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("HOST STATUS");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("DBMS INSTANCE");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("PORT");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("FILESYSTEM TYPE");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("I/O SERVER TYPE");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("INSTANCE STATUS");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj = 0;
				fieldtypes = (char **) malloc(sizeof(char *) * num_fields);
				if (!fieldtypes) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtypes");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_INT);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_INT);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				if (oph_json_add_grid(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, "DBMS Instance List", NULL, jsonkeys, num_fields, fieldtypes, num_fields)) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID error\n");
					logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID error\n");
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
					return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
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
			}
			break;
		case 1:
			printf("+----------------------+----------+--------+-------------+\n");
			printf("| %-20s | %-8s | %-6s | %-11s |\n", "HOST NAME", "MEMORY", "CORES", "HOST STATUS");
			printf("+----------------------+----------+--------+-------------+\n");

			// SET TABLE COLUMN FOR JSON
			if (objkey_printable) {
				char **jsonkeys = NULL;
				char **fieldtypes = NULL;
				int num_fields = 4, iii, jjj = 0;
				jsonkeys = (char **) malloc(sizeof(char *) * num_fields);
				if (!jsonkeys) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "keys");
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jsonkeys[jjj] = strdup("HOST NAME");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("MEMORY");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("CORES");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				jsonkeys[jjj] = strdup("HOST STATUS");
				if (!jsonkeys[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "key");
					for (iii = 0; iii < jjj; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj = 0;
				fieldtypes = (char **) malloc(sizeof(char *) * num_fields);
				if (!fieldtypes) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtypes");
					for (iii = 0; iii < num_fields; iii++)
						if (jsonkeys[iii])
							free(jsonkeys[iii]);
					if (jsonkeys)
						free(jsonkeys);
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_INT);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_INT);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				jjj++;
				fieldtypes[jjj] = strdup(OPH_JSON_STRING);
				if (!fieldtypes[jjj]) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
					logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "fieldtype");
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
					return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
				}
				if (oph_json_add_grid(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, "Host List", NULL, jsonkeys, num_fields, fieldtypes, num_fields)) {
					pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID error\n");
					logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID error\n");
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
					return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
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
			}

			break;
		default:
			pmesg(LOG_WARNING, __FILE__, __LINE__, "INSTANCES level unrecognized\n");
			return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	//retrieve information INSTANCES
	if (oph_odb_stge_find_instances_information(oDB, level, hostname, partition_name, fs_type, ioserver_type, host_status, dbms_status, &info_instances)) {
		pmesg(LOG_WARNING, __FILE__, __LINE__, "Unable to retreive information INSTANCES\n");
		logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_INSTANCES_NOT_FOUND);
		mysql_free_result(info_instances);
		return OPH_ANALYTICS_OPERATOR_SUCCESS;
	}
	//Empty set
	if (!(num_rows = mysql_num_rows(info_instances))) {
		pmesg(LOG_WARNING, __FILE__, __LINE__, "No rows find by query\n");
		logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NO_ROWS_FOUND);
		mysql_free_result(info_instances);
		return OPH_ANALYTICS_OPERATOR_SUCCESS;
	}

	MYSQL_ROW row;

	//For each ROW
	while ((row = mysql_fetch_row(info_instances))) {
		switch (level) {
			case 3:
				if (partition_name) {
					printf("| %-20s | %-20s | %-11s |\n", (row[0] ? row[0] : "-"), (row[1] ? row[1] : "-"), (row[2] ? row[2] : "-"));

					// ADD ROW TO JSON GRID
					if (objkey_printable) {
						char **jsonvalues = NULL;
						int num_fields = 3, iii, jjj;
						jsonvalues = (char **) calloc(num_fields, sizeof(char *));
						if (!jsonvalues) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "values");
							return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
						}
						for (jjj = 0; jjj < num_fields; jjj++) {
							jsonvalues[jjj] = strdup(row[jjj] ? row[jjj] : "");
							if (!jsonvalues[jjj]) {
								pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
								logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "value");
								for (iii = 0; iii < jjj; iii++)
									if (jsonvalues[iii])
										free(jsonvalues[iii]);
								if (jsonvalues)
									free(jsonvalues);
								return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
							}
						}
						if (oph_json_add_grid_row(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, jsonvalues)) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID ROW error\n");
							logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID ROW error\n");
							for (iii = 0; iii < num_fields; iii++)
								if (jsonvalues[iii])
									free(jsonvalues[iii]);
							if (jsonvalues)
								free(jsonvalues);
							return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
						}
						for (iii = 0; iii < num_fields; iii++)
							if (jsonvalues[iii])
								free(jsonvalues[iii]);
						if (jsonvalues)
							free(jsonvalues);
					}

				} else {
					printf("| %-20s |\n", (row[0] ? row[0] : "-"));

					// ADD ROW TO JSON GRID
					if (objkey_printable) {
						char **jsonvalues = NULL;
						int num_fields = 1, iii, jjj;
						jsonvalues = (char **) calloc(num_fields, sizeof(char *));
						if (!jsonvalues) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "values");
							return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
						}
						for (jjj = 0; jjj < num_fields; jjj++) {
							jsonvalues[jjj] = strdup(row[jjj] ? row[jjj] : "");
							if (!jsonvalues[jjj]) {
								pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
								logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "value");
								for (iii = 0; iii < jjj; iii++)
									if (jsonvalues[iii])
										free(jsonvalues[iii]);
								if (jsonvalues)
									free(jsonvalues);
								return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
							}
						}
						if (oph_json_add_grid_row(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, jsonvalues)) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID ROW error\n");
							logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID ROW error\n");
							for (iii = 0; iii < num_fields; iii++)
								if (jsonvalues[iii])
									free(jsonvalues[iii]);
							if (jsonvalues)
								free(jsonvalues);
							return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
						}
						for (iii = 0; iii < num_fields; iii++)
							if (jsonvalues[iii])
								free(jsonvalues[iii]);
						if (jsonvalues)
							free(jsonvalues);
					}

				}
				break;
			case 2:
				printf("| %-20s | %-11s | %-14s | %-8s | %-15s | %-20s | %-15s |\n", (row[0] ? row[0] : "-"), (row[1] ? row[1] : "-"), (row[2] ? row[2] : "-"), (row[3] ? row[3] : "-"),
				       (row[4] ? (row[4][0] == '0' ? OPH_COMMON_IO_FS_LOCAL : OPH_COMMON_IO_FS_GLOBAL) : "-"), (row[5] ? row[5] : "-"), (row[6] ? row[6] : "-"));

				// ADD ROW TO JSON GRID
				if (objkey_printable) {
					char **jsonvalues = NULL;
					int num_fields = 7, iii, jjj;
					jsonvalues = (char **) calloc(num_fields, sizeof(char *));
					if (!jsonvalues) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "values");
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					for (jjj = 0; jjj < num_fields; jjj++) {
						if (jjj == 4)
							jsonvalues[jjj] = strdup((row[jjj] ? (row[jjj][0] == '0' ? OPH_COMMON_IO_FS_LOCAL : OPH_COMMON_IO_FS_GLOBAL) : ""));
						else
							jsonvalues[jjj] = strdup(row[jjj] ? row[jjj] : "");
						if (!jsonvalues[jjj]) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "value");
							for (iii = 0; iii < jjj; iii++)
								if (jsonvalues[iii])
									free(jsonvalues[iii]);
							if (jsonvalues)
								free(jsonvalues);
							return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
						}
					}
					if (oph_json_add_grid_row(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, jsonvalues)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID ROW error\n");
						logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID ROW error\n");
						for (iii = 0; iii < num_fields; iii++)
							if (jsonvalues[iii])
								free(jsonvalues[iii]);
						if (jsonvalues)
							free(jsonvalues);
						return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					}
					for (iii = 0; iii < num_fields; iii++)
						if (jsonvalues[iii])
							free(jsonvalues[iii]);
					if (jsonvalues)
						free(jsonvalues);
				}
				break;
			case 1:
				printf("| %-20s | %-8s | %-6s | %-11s |\n", (row[0] ? row[0] : "-"), (row[1] ? row[1] : "-"), (row[2] ? row[2] : "-"), (row[3] ? row[3] : "-"));

				// ADD ROW TO JSON GRID
				if (objkey_printable) {
					char **jsonvalues = NULL;
					int num_fields = 4, iii, jjj;
					jsonvalues = (char **) calloc(num_fields, sizeof(char *));
					if (!jsonvalues) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
						logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "values");
						return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
					}
					for (jjj = 0; jjj < num_fields; jjj++) {
						jsonvalues[jjj] = strdup(row[jjj] ? row[jjj] : "");
						if (!jsonvalues[jjj]) {
							pmesg(LOG_ERROR, __FILE__, __LINE__, "Error allocating memory\n");
							logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_MEMORY_ERROR_INPUT, "value");
							for (iii = 0; iii < jjj; iii++)
								if (jsonvalues[iii])
									free(jsonvalues[iii]);
							if (jsonvalues)
								free(jsonvalues);
							return OPH_ANALYTICS_OPERATOR_MEMORY_ERR;
						}
					}
					if (oph_json_add_grid_row(handle->operator_json, OPH_JSON_OBJKEY_INSTANCES, jsonvalues)) {
						pmesg(LOG_ERROR, __FILE__, __LINE__, "ADD GRID ROW error\n");
						logging(LOG_WARNING, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, "ADD GRID ROW error\n");
						for (iii = 0; iii < num_fields; iii++)
							if (jsonvalues[iii])
								free(jsonvalues[iii]);
						if (jsonvalues)
							free(jsonvalues);
						return OPH_ANALYTICS_OPERATOR_UTILITY_ERROR;
					}
					for (iii = 0; iii < num_fields; iii++)
						if (jsonvalues[iii])
							free(jsonvalues[iii]);
					if (jsonvalues)
						free(jsonvalues);
				}

				break;
			default:
				pmesg(LOG_WARNING, __FILE__, __LINE__, "INSTANCES level unrecognized\n");
				mysql_free_result(info_instances);
				return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
		}
	}
	mysql_free_result(info_instances);

	switch (level) {
		case 3:
			if (partition_name)
				printf("+----------------------+----------------------+-------------+\n");
			else
				printf("+----------------------+\n");
			break;
		case 2:
			printf("+----------------------+-------------+----------------+----------+-----------------+----------------------+-----------------+\n");
			break;
		case 1:
			printf("+----------------------+----------+--------+-------------+\n");
			break;
		default:
			pmesg(LOG_WARNING, __FILE__, __LINE__, "INSTANCES level unrecognized\n");
			return OPH_ANALYTICS_OPERATOR_INVALID_PARAM;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_reduce(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int task_destroy(oph_operator_struct * handle)
{
	if (!handle || !handle->operator_handle) {
		pmesg(LOG_ERROR, __FILE__, __LINE__, "Null Handle\n");
		logging(LOG_ERROR, __FILE__, __LINE__, OPH_GENERIC_CONTAINER_ID, OPH_LOG_OPH_INSTANCES_NULL_OPERATOR_HANDLE);
		return OPH_ANALYTICS_OPERATOR_NULL_OPERATOR_HANDLE;
	}

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}

int env_unset(oph_operator_struct * handle)
{
	//If NULL return success; it's already free
	if (!handle || !handle->operator_handle)
		return OPH_ANALYTICS_OPERATOR_SUCCESS;

	oph_odb_disconnect_from_ophidiadb(&((OPH_INSTANCES_operator_handle *) handle->operator_handle)->oDB);
	oph_odb_free_ophidiadb(&((OPH_INSTANCES_operator_handle *) handle->operator_handle)->oDB);
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname) {
		free((char *) ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->hostname = NULL;
	}
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name) {
		free((char *) ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->partition_name = NULL;
	}
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status) {
		free((char *) ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->host_status = NULL;
	}
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status) {
		free((char *) ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->dbms_status = NULL;
	}
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys) {
		oph_tp_free_multiple_value_param_list(((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys, ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys_num);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->objkeys = NULL;
	}
	if (((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type) {
		free((char *) ((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type);
		((OPH_INSTANCES_operator_handle *) handle->operator_handle)->ioserver_type = NULL;
	}
	free((OPH_INSTANCES_operator_handle *) handle->operator_handle);
	handle->operator_handle = NULL;

	return OPH_ANALYTICS_OPERATOR_SUCCESS;
}
