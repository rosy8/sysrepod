/*
 * DataStoreSet.cpp
 *
 * License : Apache 2.0
 *
 *  Created on: Jan 31, 2015
 *      Author: Niraj Sharma
 *      Cisco Systems, Inc.
 */

// DO NOT USE GLOBAL LOGGING MECHANISM IN THIS FILE

#include <string.h>
#include "global.h"
#include "DataStoreSet.h"


DataStoreSet::DataStoreSet() {
	dataStoreList = NULL;
	count = 0;
	maxNumber = 0;
	globalTransactionCounter = 1;

}

DataStoreSet::~DataStoreSet()
{
   int i;

   pthread_mutex_destroy (&dsMutex);
   for (i=0; i < maxNumber; i++){
	   delete dataStoreList[i];
   }
   delete dataStoreList;
}

// Do not use log system as it is not created yet.

bool
DataStoreSet::initialize (int number)
{
	int i;

	if (pthread_mutex_init(&dsMutex, NULL) != 0)
	{
		// Data store mutex init failed.
		printf ("Error: Failed to init Mutex for DataStore Set.\n");
		return false;
	}
	dataStoreList = new DataStore *[number];
	if (dataStoreList == NULL) {
		printf ("Error: Unable to allocate memory for data store set.\n");
		return false;
	}
	maxNumber = number;
	for (i = 0; i < maxNumber; i++){
		dataStoreList[i] = NULL;
	}
	return true;
}

// Can use log system
bool
DataStoreSet::addDataStoreFromString (char *name, char *xml)
{
	if(pthread_mutex_lock(&dsMutex)){
		printf ("Unable to lock data store set.\n");
		return false;
	}
	if (count == maxNumber) {
		printf ("Limit of %d data stores reached. Can not add more data stores.\n", maxNumber);
		pthread_mutex_unlock(&dsMutex);
		return false;
	}
	if(getDataStore_noLock(name)){
		printf ("Duplicate Data Store Names are not allowed: %s\n", name);
		pthread_mutex_unlock(&dsMutex);
		return false;
	}
    dataStoreList[count] = new DataStore(name);
    if (dataStoreList[count] == NULL || !dataStoreList[count]->initialize(xml)){
    	delete dataStoreList[count];
    	dataStoreList[count] = NULL;
    	printf ("Warning in creating a new data store: %s\n", name);
    	pthread_mutex_unlock(&dsMutex);
    	return false;
    }
    count++;
    pthread_mutex_unlock(&dsMutex);
    return true;
}

// Do not use log system as it may not be created yet.
bool
DataStoreSet::addDataStoreFromFile (char *name, char *inFile, char *checkDir, char *yangDir)
{
	if(pthread_mutex_lock(&dsMutex)){
		printf ("Unable to lock data store set.\n");
		return false;
	}
	if (count == maxNumber) {
		printf ("Limit of %d data stores reached. Can not add more data stores.\n", maxNumber);
		pthread_mutex_unlock(&dsMutex);
		return false;
	}
	if(getDataStore_noLock(name)){
		printf ("Duplicate Data Store Names are not allowed: %s\n", name);
		pthread_mutex_unlock(&dsMutex);
		return false;
	}
    dataStoreList[count] = new DataStore(name, inFile, checkDir, yangDir);
    if (dataStoreList[count] == NULL || !dataStoreList[count]->initialize()){
    	delete dataStoreList[count];
    	dataStoreList[count] = NULL;
    	printf ("Warning in creating a new data store: %s\n", name);
    	pthread_mutex_unlock(&dsMutex);
    	return false;
    }
    count++;
    pthread_mutex_unlock(&dsMutex);
    return true;
}

// Do not use log system as it may not be created yet.
DataStore *
DataStoreSet::getDataStore (char *name)
{
	int i;

	if(pthread_mutex_lock(&dsMutex)){
		printf ("Unable to lock data store set.\n");
		return NULL;
	}
	for (i=0; i < count; i++){
		if (strcmp(name, dataStoreList[i]->name) == 0){
			pthread_mutex_unlock(&dsMutex);
			return dataStoreList[i];
		}
	}
	pthread_mutex_unlock(&dsMutex);
	return NULL;
}

DataStore *
DataStoreSet::getDataStore_noLock (char *name)
{
	int i;
	for (i=0; i < count; i++){
		if (strcmp(name, dataStoreList[i]->name) == 0){
			return dataStoreList[i];
		}
	}
	return NULL;
}

char *
DataStoreSet::getList (void)
{
	int i;
	char *list;
	int requiredSize = 0;

	if(pthread_mutex_lock(&dsMutex)){
		printf ("Unable to lock data store set.\n");
		return NULL;
	}
	if (count == 0){
		pthread_mutex_unlock(&dsMutex);
		return NULL;
	}
	for (i = 0; i < count; i++){
		requiredSize = requiredSize + strlen (dataStoreList[i]->name) + strlen ("<dataStore></dataStore>");
	}
	requiredSize = requiredSize + strlen ("<dataStores></dataStores>") + 10;
	list = (char *) malloc (requiredSize);
	if (list) {
	   sprintf (list, "<dataStores>");
	   for (i=0; i < count; i++){
		   strcat (list, "<dataStore>");
		   strcat (list, dataStoreList[i]->name);
		   strcat (list, "</dataStore>");
	   }
	   strcat (list, "</dataStores>");
	} else {
		printf ("Unable to allocate space for Data Store list.");
	}
	pthread_mutex_unlock(&dsMutex);
	return list;
}

int
DataStoreSet::deleteDataStore (struct ClientInfo *cinfo, ClientSet *cset, char *name)
{
	int i;
	int retValue = 0;
	DataStore *ds;

	if(pthread_mutex_lock(&dsMutex)){
		printf ("Unable to lock data store set.\n");
		return -1;
	}
	for (i=0; i < count; i++){
		if (strcmp(name, dataStoreList[i]->name) == 0){
			// Clients get a handle to a data store by locking DataStoreSet first.
			// DataStoreSet is locked above, so no additional client can get a handle on this data store.
			// Now we can check if any client is using this data store. If in use, we can not delete this data store.
			if (cset->isDataStoreInUse (dataStoreList[i])){
				printf ("Data Store %s is in use, can not be deleteted.\n", name);
				retValue = -1;
				break;
			}
			if (dataStoreList[i]->lockDS(cinfo)){
				retValue = -1;
				break;
			}
			// remove from the list
			ds = dataStoreList[i];
			dataStoreList[i] = NULL;
			// shift array up
			if (i < count -1){
			   memmove (&(dataStoreList[i]), &(dataStoreList[i+1]), sizeof (DataStore *)* (count - i - 1));
			}
            count --;
            ds->unlockDS(cinfo);
            delete (ds);
            retValue = 1;
            break;
		}
	}
	pthread_mutex_unlock(&dsMutex);
	return retValue;
}

int
DataStoreSet::getNextTransactionId (char *log)
{
	int value;
	if(pthread_mutex_lock(&dsMutex)){
		sprintf (log, "Unable to lock data store set.");
		return 0;
	}
	globalTransactionCounter ++;
	value = globalTransactionCounter;
	pthread_mutex_unlock(&dsMutex);
	return value;
}

int
DataStoreSet::copyDataStore (struct ClientInfo *cinfo, char *fromDS, char *toDS, char *printBuff, int printBuffSize)
{
	DataStore *from, *to;
	xmlDocPtr duplicate;
	int retValue;

	if(pthread_mutex_lock(&dsMutex)){
		sprintf (printBuff, "Unable to lock data store set.");
		return 0;
	}
	from = getDataStore_noLock (fromDS);
	if (from == NULL){
		sprintf (printBuff, "Could not locate FROM_Data_Store %s", fromDS);
		pthread_mutex_unlock(&dsMutex);
		return 0;
	}
	if (from->lockDS(cinfo)){
		sprintf (printBuff, "Unable to lock FROM_Data_Store %s", fromDS);
		pthread_mutex_unlock(&dsMutex);
		return 0;
	}
	if (from->doc == NULL){
		sprintf (printBuff, "DOM tree in FROM_Data_Store %s is NULL", fromDS);
		from->unlockDS(cinfo);
		pthread_mutex_unlock(&dsMutex);
		return 0;
	}
	to = getDataStore_noLock (toDS);
	if (to == NULL){
		sprintf (printBuff, "Unable to locate TO_Data_store %s.", toDS);
		from->unlockDS(cinfo);
		pthread_mutex_unlock(&dsMutex);
		return 0;
	}
	if (to->lockDS(cinfo)){
		sprintf (printBuff, "Unable to lock TO_Data_Store %s", toDS);
		from->unlockDS(cinfo);
		pthread_mutex_unlock(&dsMutex);
		return 0;
	}

	// create a duplicate of from->doc
	duplicate = xmlCopyDoc (from->doc, 1);
	if (duplicate){
	    xmlFreeDoc (to->doc);
	    to->doc = duplicate;
	    retValue = 1;
	}else {
		sprintf (printBuff, "Unable to create duplicate DOM tree from data store %s", fromDS);
		retValue = 0;
	}

	from->unlockDS(cinfo);
	to->unlockDS(cinfo);

	pthread_mutex_unlock(&dsMutex);
	return retValue;
}

