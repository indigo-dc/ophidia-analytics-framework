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

#ifndef __OPH_ODB_FS_QUERY_H__
#define __OPH_ODB_FS_QUERY_H__

#define MYSQL_QUERY_FS_RETRIEVE_ROOT_ID 		"SELECT idfolder FROM folder WHERE idparent IS NULL"
#define MYSQL_QUERY_FS_PATH_PARSING_ID 			"SELECT idfolder FROM folder WHERE idparent=%d AND foldername='%s'"
#define MYSQL_QUERY_FS_IS_VISIBLE_CONTAINER 		"SELECT idcontainer FROM container WHERE idfolder=%d AND containername='%s' AND hidden=0"
#define MYSQL_QUERY_FS_IS_HIDDEN_CONTAINER 		"SELECT idcontainer FROM container WHERE idfolder=%d AND containername='%s' AND hidden=1"
#define MYSQL_QUERY_FS_UNIQUENESS 			"SELECT folder.foldername FROM folder WHERE folder.idparent=%d AND folder.foldername='%s' UNION SELECT container.containername FROM container WHERE container.idfolder=%d AND container.hidden=0 AND container.containername='%s'"
#define MYSQL_QUERY_FS_UNIQUENESS_HIDDEN 		"SELECT container.containername FROM container WHERE container.idfolder=%d AND container.hidden=1 AND container.containername='%s'"
#define MYSQL_QUERY_FS_EMPTINESS 			"SELECT folder.foldername FROM folder WHERE folder.idparent=%d UNION SELECT container.containername FROM container WHERE container.idfolder=%d"

#define MYSQL_QUERY_FS_RETRIEVE_SESSION_FOLDER_ID 	"SELECT idfolder FROM session WHERE sessionid = '%s';"
#define MYSQL_QUERY_FS_RETRIEVE_CONTAINER_FOLDER_ID 	"SELECT idfolder FROM container WHERE idcontainer = %d AND hidden != 1;"
#define MYSQL_QUERY_FS_RETRIEVE_CONTAINER_FOLDER_ID2 	"SELECT idfolder FROM container WHERE idcontainer = %d;"
#define MYSQL_QUERY_FS_RETRIEVE_PARENT_FOLDER_ID 	"SELECT idparent FROM folder WHERE idfolder = %d;"

#define MYSQL_QUERY_FS_RETRIEVE_PARENT_FOLDER 		"SELECT idparent, foldername FROM folder WHERE idfolder = %d;"
#define MYSQL_QUERY_FS_CONTAINER_STATUS 		"UPDATE container SET container.hidden= %d WHERE idcontainer = %d;"
#define MYSQL_QUERY_FS_MV 				"UPDATE container SET container.idfolder=%d, container.containername = '%s' WHERE container.idcontainer=%d;"
#define MYSQL_QUERY_FS_LIST_0 				"SELECT foldername, idfolder FROM folder WHERE folder.idparent=%d;"
#define MYSQL_QUERY_FS_LIST_1 				"SELECT foldername AS name, 1 AS type FROM folder WHERE folder.idparent=%d UNION SELECT containername AS name, 2 AS type FROM container WHERE container.idfolder=%d AND hidden=0;"
#define MYSQL_QUERY_FS_LIST_1_WC 			"SELECT containername AS name, 2 AS type FROM container WHERE container.idfolder=%d AND hidden=0 AND containername LIKE '%s';"
#define MYSQL_QUERY_FS_LIST_1_H 			"SELECT foldername AS name, 1 AS type FROM folder WHERE folder.idparent=%d UNION SELECT containername AS name, 2 AS type FROM container WHERE container.idfolder=%d AND hidden=0 UNION SELECT containername AS name, 3 AS type FROM container WHERE container.idfolder=%d AND hidden=1"
#define MYSQL_QUERY_FS_LIST_1_H_WC 			"SELECT containername AS name, 2 AS type FROM container WHERE container.idfolder=%d AND containername LIKE '%s' AND hidden=0 UNION SELECT containername AS name, 3 AS type FROM container WHERE container.idfolder=%d AND hidden=1 AND containername LIKE '%s';"
#define MYSQL_QUERY_FS_LIST_2  				"SELECT foldername AS name, 1 AS type, NULL AS idcontainer, NULL AS iddatacube, NULL AS description FROM folder WHERE folder.idparent=%d UNION SELECT containername AS name, 2 AS type, datacube.idcontainer, iddatacube, datacube.description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden=0 ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_WC  			"SELECT containername AS name, 2 AS type, datacube.idcontainer, iddatacube, datacube.description AS description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden=0 AND containername LIKE '%s' ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_H  			"SELECT foldername AS name, 1 AS type, NULL AS idcontainer, NULL AS iddatacube, NULL AS description FROM folder WHERE folder.idparent=%d UNION SELECT containername AS name, 2 AS type, datacube.idcontainer, iddatacube, datacube.description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden= 0 UNION SELECT containername AS name, 3 AS type, datacube.idcontainer, iddatacube, datacube.description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden=1 ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_H_WC  			"SELECT containername AS name, 2 AS type, datacube.idcontainer, iddatacube, datacube.description AS description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden= 0 AND containername LIKE '%s' UNION SELECT containername AS name, 3 AS type, datacube.idcontainer, iddatacube, datacube.description FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer WHERE container.idfolder=%d AND hidden=1 AND containername LIKE '%s' ORDER BY idcontainer, iddatacube ASC;"

#define MYSQL_QUERY_FS_LIST_2_FILTER  			"SELECT foldername AS name, 1 AS type, NULL AS idcontainer, NULL AS iddatacube, NULL AS description, NULL AS level, NULL AS measure, NULL AS uri, NULL AS idinput FROM folder WHERE folder.idparent=%d UNION SELECT DISTINCT containername AS name, 2 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN '%s' ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=0 %s ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_WC_FILTER  		"SELECT DISTINCT containername AS name, 2 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description AS description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN '%s' ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=0 AND containername LIKE '%s' %s ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_H_FILTER  		"SELECT foldername AS name, 1 AS type, NULL AS idcontainer, NULL AS iddatacube, NULL AS description, NULL AS level, NULL AS measure, NULL AS uri, NULL AS idinput FROM folder WHERE folder.idparent=%d UNION SELECT DISTINCT containername AS name, 2 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN '%s' ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=0 %s UNION SELECT DISTINCT containername AS name, 3 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN '%s' ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=1 %s ORDER BY idcontainer, iddatacube ASC;"
#define MYSQL_QUERY_FS_LIST_2_H_WC_FILTER  		"SELECT DISTINCT containername AS name, 2 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description AS description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN '%s' ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=0 AND containername LIKE '%s' %s UNION SELECT DISTINCT containername AS name, 3 AS type, datacube.idcontainer, datacube.iddatacube, datacube.description, level, measure, uri, CASE WHEN task.inputnumber > 1 THEN NULL ELSE hasinput.iddatacube END AS idinput FROM container LEFT OUTER JOIN datacube ON datacube.idcontainer=container.idcontainer LEFT OUTER JOIN source ON datacube.idsource = source.idsource LEFT OUTER JOIN task ON task.idoutputcube = datacube.iddatacube LEFT OUTER JOIN hasinput ON hasinput.idtask= task.idtask WHERE container.idfolder=%d AND hidden=1 AND containername LIKE '%s'  %s ORDER BY idcontainer, iddatacube ASC;"

#define MYSQL_QUERY_FS_MKDIR				"INSERT INTO folder (idfolder,idparent,foldername) VALUES (null,%d,'%s')"
#define MYSQL_QUERY_FS_RMDIR				"DELETE FROM folder WHERE folder.idfolder=%d"
#define MYSQL_QUERY_FS_MVDIR				"UPDATE folder SET folder.idparent=%d,folder.foldername='%s' WHERE folder.idfolder=%d"

#define MYSQL_QUERY_FS_UPDATE_OPHIDIADB_CONTAINER 	"INSERT INTO `container` (`idfolder`, `idparent`, `containername`, `operator`) VALUES (%d, %d, '%s', '%s')"
#define MYSQL_QUERY_FS_UPDATE_OPHIDIADB_CONTAINER_2 	"INSERT INTO `container` (`idfolder`, `containername`, `operator`) VALUES (%d, '%s', '%s')"
#define MYSQL_QUERY_FS_UPDATE_OPHIDIADB_CONTAINER_3 	"INSERT INTO `container` (`idfolder`, `idparent`, `containername`, `operator`, `idvocabulary`) VALUES (%d, %d, '%s', '%s', %d)"
#define MYSQL_QUERY_FS_UPDATE_OPHIDIADB_CONTAINER_4 	"INSERT INTO `container` (`idfolder`, `containername`, `operator`, `idvocabulary`) VALUES (%d, '%s', '%s', %d)"
#define MYSQL_QUERY_FS_DELETE_OPHIDIADB_CONTAINER 	"DELETE FROM `container` WHERE `idcontainer` = %d"
#define MYSQL_QUERY_FS_RETRIEVE_EMPTY_CONTAINER 	"SELECT count(*) FROM container INNER JOIN datacube ON container.idcontainer = datacube.idcontainer WHERE container.idcontainer = %d"
#define MYSQL_QUERY_FS_RETRIEVE_CONTAINER_ID 		"SELECT idcontainer from `container` where containername = '%s' AND idfolder = %d AND hidden = %d;"
#define MYSQL_QUERY_FS_RETRIEVE_CONTAINER_ID2 		"SELECT idcontainer from `container` where containername = '%s' AND idfolder = %d AND hidden = 0;"

#define MYSQL_QUERY_FS_UPDATE_OPHIDIADB_CONTAINER_NAME	"UPDATE `container` SET containername = CONCAT(containername, CONCAT('_', idcontainer)) WHERE idcontainer = '%d'"

#endif				/* __OPH_ODB_FS_QUERY_H__ */
