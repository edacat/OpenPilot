/**
 ******************************************************************************
 *
 * @file       $(NAMELC).h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2013.
 * @see        The GNU Public License (GPL) Version 3
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup UAVObjectsPlugin UAVObjects Plugin
 * @{
 *   
 * @note       Object definition file: $(XMLFILE). 
 *             This is an automatically generated file.
 *             DO NOT modify manually.
 *
 * @brief      The UAVUObjects GCS plugin 
 *****************************************************************************/
/* 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef $(NAMEUC)_H
#define $(NAMEUC)_H

#include "uavdataobject.h"
#include "uavobjectmanager.h"

class $(NAME): public UAVDataObject
{
public:
    // Field structure
    typedef struct {
$(DATAFIELDS)
    } __attribute__((packed)) DataFields;

    // Field information
$(DATAFIELDINFO)
  
    // Constants
    static const opuint32 OBJID = $(OBJIDHEX);
    static const CString NAME;
    static const CString DESCRIPTION;
    static const CString CATEGORY;
    static const bool ISSINGLEINST = $(ISSINGLEINST);
    static const bool ISSETTINGS = $(ISSETTINGS);
    static const opuint32 NUMBYTES = sizeof(DataFields);

    // Functions
    $(NAME)();

    DataFields getData();
    void setData(const DataFields& data);
    Metadata getDefaultMetadata();
    UAVDataObject* clone(opuint32 instID);
	UAVDataObject* dirtyClone();
	
    static $(NAME)* GetInstance(UAVObjectManager* objMngr, opuint32 instID = 0);

$(PROPERTY_GETTERS)

$(PROPERTY_SETTERS)

private:
$(PROPERTY_NOTIFICATIONS)

    CL_Slot m_slotObjectUpdated;

    void emitNotifications(UAVObject*);
	
private:
    DataFields data;

    void setDefaultFieldValues();
};

#endif // $(NAMEUC)_H