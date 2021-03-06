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

#ifndef __OPH_INSTANCES_OPERATOR_H
#define __OPH_INSTANCES_OPERATOR_H

//Operator specific headers
#include "oph_ophidiadb_main.h"
#include "oph_common.h"

#define OPH_INSTANCES_ALL_FS_TYPE 2

/**
 * \brief Structure of parameters needed by the operator OPH_INSTANCES. It shows information about hosts and dbms instances
 * \param oDB Contains the parameters and the connection to OphidiaDB
 * \param level Level of verbosity 1: only host, 2: only dbms instances (of a specific host), 3: host and dbms instances
 * \param fs_type Shows only DBMS on dedicated disk (local - 0) or GPFS (global - 1)
 * \param ioserver_type Type of I/O server
 * \param hostname Name of the host to filter on
 * \param partition_name Name of the host partition to filter on
 * \param dbms_status Status of the dbms instance to filter on
 * \param host_status Status of the host to filter on
 * \param objkeys OPH_JSON objkeys to be included in output JSON file.
 * \param objkeys_num Number of objkeys.
 */
struct _OPH_INSTANCES_operator_handle {
	ophidiadb oDB;
	int level;
	int fs_type;
	char *ioserver_type;
	char *hostname;
	char *partition_name;
	char *dbms_status;
	char *host_status;
	char **objkeys;
	int objkeys_num;
};
typedef struct _OPH_INSTANCES_operator_handle OPH_INSTANCES_operator_handle;

#endif				//__OPH_INSTANCES_OPERATOR_H
