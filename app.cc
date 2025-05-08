
/*
 * CMPT 464 Assignment 2 - Distributed Data Storage System
 * Author: Ryan-Jay, Darion, Adam, Kody
 * Date: March 30, 2025
 *
 * This program implements a wireless distributed storage system.
 * It supports discovering neighbors, storing/retrieving/deleting 
 * information records,and interacting with a user through UART. 
 * The application is designed for PicOS.
 */

#include "sysio.h"
#include "ser.h"
#include "serf.h"
#include "phys_cc1350.h"
#include "plug_null.h"
#include "tcv.h"
#include "time.h"

// Constants
#define SECOND 1024       // 1 second in PicOS time units
#define MAX_RECORDS 40    // Max number of records that can be stored
#define MAX_PACKET_LENGTH 250  // Max packet size including headers
#define RECORD_LENGTH 20       // Max length of a record string (inc '\0')
#define MAX_NEIGHBORS 25      // Max number of neighbors to track

// Structure to store an information record
// Includes timestamp, owner node ID, and the actual record string
typedef struct {
    lword timeStamp;
    byte ownerID;
    char recordMessage[RECORD_LENGTH];
}record;

// Global Variables
record database[MAX_RECORDS]; // Local database for storing records
int records_Stored = 0;     // Counter of current stored records
int recordCount = 0;      // Alternative record counter for display 
int currentRecord;     // Index of current record being operated on

int nodeId = 1;       // Unique node ID (1 to 25)
byte groupId = 1;   // Group ID this node belongs to
int destId = 0;     // Destination node ID for send operations
int recordID = 0;   // Target record index for operations
                    //
int deleteResponseReceived = 0; // Flag used to track if delete response was received

char msg[20];       // General message buffer
int sfd;            // TCV stream file descriptor
record *messageP;   // Pointer used for record message manipulation

byte neighborList[MAX_NEIGHBORS];  // Stores IDs of discovered neighbors
int neighborCount = 0;            // Current count of neighbors
int roundCount = 0;         // Used in discovery rounds
int neighborIndex = 0;    // Tracks which neighbor is being interacted with
int messagesent = 0;    // Flag to check if message has been sent
int retrieve = 0;       // Flag indicating if we're expecting a retrieve

// FSM: Receiver - handles all incoming wireless messages
fsm receiver {
    char strbuffer[RECORD_LENGTH];// Buffer to hold incoming string
    byte senderId;                   // ID of sender node
    byte type;                       // Message type
    byte receiverId;                 // Intended recipient node ID
    word incomingGroup;              // Group ID of incoming message
    byte reqNumber;                  // Request number from sender

    state Rcv:
        //wait for incoming packet
        address pkt = tcv_rnp(Rcv, sfd);
        byte *p = (byte *)(pkt+1);

        // Parse the group ID (first two bytes)
        incomingGroup = ((word)p[0] << 8) | p[1];
        type = p[2];                // Message type
        reqNumber = p[3];           // Request number
        senderId = p[4];            // Sender ID
        receiverId = p[5];          // Receiver ID

        if (incomingGroup != groupId || receiverId != 0 && receiverId != nodeId) {
            tcv_endp(pkt);
            proceed Rcv;
        }
                // Receive discovery request
        if (type == 0x00) {
            // Send discovery response
            address packet = tcv_wnp(Rcv, sfd, 10);
            byte *p = (byte *)(packet+1);
            
            word g = (word)groupId;
            *p++ = (g >> 8) & 0xFF;
            *p++ = g & 0xFF;          // Group ID
            *p++ = 0x01;          // Type (always set to 1)
            *p++ = reqNumber;    	        // Request Number
            *p++ = nodeId;  // Sender ID
            *p++ = senderId;// Reviever ID (node ID that send request)
	    
            tcv_endp(packet); //send response
            tcv_endp(pkt); //end recieved packet
            proceed Rcv;
        }
		
       	// Receive discovery response
        else if (type == 0x01) {
            // Check if sender already exists in neighborList
            int exists = 0;
            for (int i = 0; i < neighborCount; i++) {
                if (neighborList[i] == senderId) {
                    exists = 1;
                    break;
                }
            }

            // Add senderId to neighborList
            if (!exists && neighborCount < MAX_NEIGHBORS) {
                neighborList[neighborCount++] = senderId;
            }
            tcv_endp(pkt);
            proceed Rcv;
        }
        
	// Create Record Response
        if (type == 0x02) {
            int i;
            byte status = 0x01;
            // Find first empty slot in the database
            for (i = 0; i < MAX_RECORDS; i++) {
                if (database[i].ownerID == 0) break;
            }
            // 
            if (i < MAX_RECORDS) {
              // Store the record with timestamp and sender ID
                database[i].ownerID = senderId;
                database[i].timeStamp = seconds();
                strncpy(database[i].recordMessage, &p[6], RECORD_LENGTH);
                recordCount++;
                records_Stored++;
                messagesent = 1;
            }
            else{
		status = 0x02;
            }  
  // Build and send a response message back
		address packet = tcv_wnp(Rcv, sfd, 30);
                byte *p = (byte *)(packet);
                
                word g = (word)groupId;
                *p++ = (g >> 8) & 0xFF;
                *p++ = g & 0xFF;                // Group ID
                *p++ = 0x05;                    // Type (always set to 1)
                *p++ =  (byte)(rand() % 256);   // Request Number
                *p++ = nodeId;                  // Sender ID (node ID that recieved the request)
                *p++ = senderId;                // Reviever ID (node ID that send request)
                *p++ = 0x00;
           	*p++ = status;			// status
                tcv_endp(packet);
        }
        // --- Delete Record Request ---
	else if (type == 0x03) {
    		int idx = p[6];
    		byte status = 0x01;
        // Check if the index is valid and entry is not empty
	    	if (idx >= MAX_RECORDS || database[idx].ownerID == 0) {
			status = 0x03;  // already empty or invalid index
	    	} else {
          //clear the record
			database[idx].ownerID = 0;
			database[idx].timeStamp = 0;
			memset(database[idx].recordMessage, 0, RECORD_LENGTH);
			recordCount--;
	    	}
        // Send delete response
	    	address response = tcv_wnp(Rcv, sfd, 30);
	    	byte *resp = (byte *)(response + 1);
	
	    	word g = (word)groupId;
	    	*resp++ = (g >> 8) & 0xFF;
	    	*resp++ = g & 0xFF;
	    	*resp++ = 0x05;
	    	*resp++ = p[3];        // same request number
	    	*resp++ = nodeId;
	    	*resp++ = senderId;
	    	*resp++ = idx;
	    	*resp++ = status;
		*resp++ = 0x00; // padding
		memset(resp, 0, RECORD_LENGTH); // blank record section for delete
	    	tcv_endp(response);
	    	tcv_endp(pkt);
	    	proceed Rcv;
	}

        // --- Retrieve Record Request ---
        else if (type == 0x04) {
            // record id
            int idx = p[6];

            //send retrieve response
            address response = tcv_wnp(Rcv, sfd, 30);
            byte *resp = (byte *)(response + 1);
            word g = (word)groupId;
            *resp++ = (g >> 8) & 0xFF;
            *resp++ = g & 0xFF;
            *resp++ = 0x05;         //type = response     
            *resp++ = (byte)(rand() % 256);
            *resp++ = nodeId;
            *resp++ = senderId;
            *resp++ = idx;

            if (idx < MAX_RECORDS && database[idx].ownerID != 0) {
                *resp++ = 0x01;
                strncpy(resp, database[idx].recordMessage, RECORD_LENGTH);

            } else {
                *resp++ = 0x04;
            }
            tcv_endp(response);
        }

        else if (type == 0x05) {
            int idx = p[6];
            byte status = p[7];

            if (status == 0x01) {
            	// Check if this is a retrieve (record data present)
        	if (strlen((char *)&p[8]) > 0) {
            		strcpy(strbuffer, (char *)&p[8]);
            		retrieve = 0;
            		tcv_endp(pkt);
           		proceed c_rec;
		
	        } 
	        else {
			deleteResponseReceived = 1;
	        	tcv_endp(pkt);
	            	proceed del_success;  // <-- new state for delete success
	        }
	    }
	    else if (status == 0x02){
	    	tcv_endp(pkt);
	    	proceed max_fail;
	    }
	    else if (status == 0x03) {
		deleteResponseReceived = 1;
        	tcv_endp(pkt);
        	proceed del_fail;
    	    }
            else if(status == 0x04)
            {
		tcv_endp(pkt);
            	proceed f_rec;
            }
        }
        
        tcv_endp(pkt);
        proceed Rcv;	
        
        //states printing out status updates
        state record_success:
		ser_outf(c_rec, "\r\n The record is added to the node %d", senderId);
		proceed Rcv;
        state max_fail:
		ser_outf(c_rec, "\r\n The record can't be added to the node %d", senderId);
        	proceed Rcv;
        	
        state c_rec:
		ser_outf(c_rec, "\r\n Record Received from %d: %s", senderId, strbuffer);
        	proceed Rcv;
        
        state f_rec:
		ser_outf(f_rec, "\r\n The record does not exist on node %d", senderId);
		proceed Rcv;
	state del_success:
    		ser_outf(del_success, "\r\n Record Deleted on node %d", senderId);
    		proceed Rcv;

	state del_fail:
    		ser_outf(del_fail, "\r\n The record does not exist on node %d", senderId);
    		proceed Rcv;


}

// FSM: Root - main application entry point, handles serial setup and launches processes
fsm root {
    
    state Init:
        records_Stored = 0;
        
        // Allocate memory for message buffer
        messageP = (record *) umalloc(sizeof(record));
        
        // Set physical layer and open TCV interface
        phys_cc1350(0, MAX_PACKET_LENGTH);
        tcv_plug(0, &plug_null);
        sfd = tcv_open(NONE, 0, 0);
	      // Check if TCV interface opens successfully
        if (sfd < 0) {
            diag("Cannot open tcv interface");
            halt();
        }
	tcv_control(sfd, PHYSOPT_ON,NULL);

        runfsm receiver;
        proceed Menu;
    // Show user menu options via UART
    state Menu:
        ser_outf(Menu, "\r\nGroup %d Device #%d (%d/%d records)\r\n"
                       "(G)roup ID\r\n"
                       "(N)ew device ID\r\n"
                       "(F)ind neighbors\r\n"
                       "(C)reate record on neighbor\r\n"
                       "(D)elete record on neighbor\r\n"
                       "(R)etrieve record from neighbor\r\n"
                       "(S)how local records\r\n"
                       "R(e)set local storage\r\n\r\n"
                       "Selection: ", groupId, nodeId, recordCount, MAX_RECORDS);

    state Get_Choice:
        char choice;
        ser_inf(Get_Choice, "%c", &choice);

        switch (choice) {
            case 'G': case 'g':
                proceed GroupID;
            case 'N': case 'n':
                proceed Set_NodeID;
            case 'F': case 'f':
                proceed Find_Neighbors;
            case 'C': case 'c':
                proceed Create_Record;
            case 'D': case 'd':
                proceed Delete_Record;
            case 'R': case 'r':
                proceed Retrieve_Record;
            case 'S': case 's':
                proceed Show_Records;
            case 'E': case 'e':
            	proceed Reset_Record;
		
        }
        proceed Menu;

    // G
    state GroupID:
        ser_outf(GroupID, "New Group ID: ");
    state Get_GroupID:
        ser_inf(Get_GroupID, "%d", &groupId);
        proceed Menu;

    // N
    state Set_NodeID:
        ser_outf(Set_NodeID, "New Node ID (1-25):");
    state Get_Node_ID:
        ser_inf(Get_Node_ID, "%d", &nodeId);
        if (nodeId > 25 || nodeId < 1) {
            proceed Set_NodeID;
        }
        proceed Menu;
        
    /*------------------------------------------------------------------------------------*/
    // F
    state Find_Neighbors:
        neighborCount = 0;
        roundCount = 0;
        proceed Round;
    state Round:
        if (roundCount < 2){
            address packet = tcv_wnp(Round, sfd, 10);
            char *p = (char *)(packet+1);
            
            word g = (word)groupId;
            *p++ = (g >> 8) & 0xFF;
            *p++ = g & 0xFF;                // Group ID
            *p++ = 0x00;                    // Type: Discovery Request
            *p++ = (byte)(rand() % 256);    // Request Number
            *p++ = nodeId;                  // Sender ID
            *p++ = 0x00;                    // Receiver ID
            tcv_endp(packet);
            
            roundCount++;
            delay(3 * SECOND, Round);
            release;
        }
        else {
            proceed Print_Neighbors;
    	}
    state Print_Neighbors:
    	if (neighborCount == 0) {
            ser_outf(Print_Neighbors, "\r\nNeighbors: 0\r\n");
            proceed Menu;
        } else {
       	    neighborIndex = 0;
            ser_outf(Print_Neighbors, "\r\nNeighbors: ");
            proceed List_Neighbors;
        }

	state List_Neighbors:
	    if (neighborIndex < neighborCount) {
	    	ser_outf(List_Neighbors, "%d ", neighborList[neighborIndex]);
	    	neighborIndex++;
	    	proceed List_Neighbors;
	    } else {
	    	ser_outf(List_Neighbors, "\r\n");
	    	proceed Menu;
	    }

    /*------------------------------------------------------------------------------------*/
    // C
    state Create_Record:
        ser_outf(Create_Record, "Destination Node (1-25):");
    state Get_Dest_ID:
        ser_inf(Get_Dest_ID, "%d", &destId);
        ser_outf(Get_Dest_ID, "Enter message: ");
    state Get_Message:
        ser_in(Get_Message, msg, RECORD_LENGTH);
        proceed Send_Node;
    state Failed:
        ser_outf(Failed, "\r\nFailed to reach destination");
        proceed Menu;

    state Send_Node:
        messageP = (record *) umalloc(sizeof(record));

        if (!messageP) {
            ser_outf(Send_Node, "\r\nMemory allocation failed.");
            proceed Menu;
        }

        messageP->ownerID = nodeId;
        strncpy(messageP->recordMessage, msg, RECORD_LENGTH);
       
        address packet = tcv_wnp(Send_Node, sfd, 30);

        char *p = (char *)(packet+1);
        
        word g = (word)groupId;
        *p++ = (g >> 8) & 0xFF;
        *p++ = g & 0xFF;
        *p++ = 0x02; // always set to 2
        *p++ = (byte)(rand() % 256);
        *p++ = nodeId;
        *p++ = destId;
         strcpy(p,messageP->recordMessage); 

        tcv_endp(packet);
        ufree(messageP);
        
    state proceedToMenu:
        ser_outf(proceedToMenu, "\rMessage Request has been sent.\n");
        delay(3 * SECOND, Menu);
        release;
        proceed Menu;

    /*------------------------------------------------------------------------------------*/

    // Retrive Node
    state Retrieve_Record:
        ser_outf(Retrieve_Record, "Destination Node (1-25):");

    state Dest_ID:
        ser_inf(Dest_ID, "%d", &destId);
        ser_outf(Dest_ID, "Record Index:");

    state Get_Record:
        ser_inf(Get_Record, "%d", &recordID);
        proceed Send_Retrive_node;

    state Retrieve_Failed:
        ser_outf(Retrieve_Failed, "\r\nFailed to reach destination");
        proceed Menu;

    state Send_Retrive_node:
        messageP = (record *) umalloc(sizeof(record));

        if (!messageP) {
            ser_outf(Send_Retrive_node, "\r\nMemory allocation failed.");
            proceed Menu;
        }

        messageP->ownerID = nodeId;
       
        address packet = tcv_wnp(Send_Retrive_node, sfd, 30);

        char *p = (char *)(packet+1);
        
        word g = (word)groupId;
        *p++ = (g >> 8) & 0xFF;
        *p++ = g & 0xFF;
        *p++ = 0x04; 
        *p++ = (byte)(rand() % 256);
        *p++ = nodeId;
        *p++ = destId;
        *p++ = recordID;
        *p = 0x00; // padding byte
        
        tcv_endp(packet);
        ufree(messageP);

    state proceedback:
        ser_outf(proceedback, "\rRecieve request has been sent.\n");
        delay(3 * SECOND, Menu);
        release;

    /*------------------------------------------------------------------------------------*/

    // D

    // Delete Node
    state Delete_Record:
        ser_outf(Delete_Record, "Destination Node (1-25):");

    state Get_DESTID:
        ser_inf(Get_DESTID, "%d", &destId);
        ser_outf(Get_DESTID, "Record Index:");

    state Get_Record_ID:
        ser_inf(Get_Record_ID, "%d", &recordID);
        proceed Send_Delete_node;

    state Send_Delete_node:
        messageP = (record *) umalloc(sizeof(record));

        if (!messageP) {
            ser_outf(Send_Delete_node, "\r\nMemory allocation failed.");
            proceed Menu;
        }

        
        messageP->ownerID = nodeId;
  
        address packet = tcv_wnp(Send_Delete_node, sfd, 30);

        char *p = (char *)(packet+1);
        
        word g = (word)groupId;
        *p++ = (g >> 8) & 0xFF;
        *p++ = g & 0xFF;
        *p++ = 0x03; 
        *p++ = (byte)(rand() % 256);
        *p++ = nodeId;
        *p++ = destId;
        *p++ = recordID;
	*p++ = 0x00; // padding byte


        tcv_endp(packet);
        ufree(messageP);

    state gobackmenu:
        deleteResponseReceived = 0;
    	ser_outf(gobackmenu, "\rDelete request has been sent.\n");
    	delay(3 * SECOND, Check_Delete_Response);
	release;

    state Check_Delete_Response:
	if (!deleteResponseReceived) {
	    proceed Retrieve_Failed;  // This already prints: "Failed to reach destination"
	} else {
	    proceed Menu;
	}


    /*------------------------------------------------------------------------------------*/
    // S
    state Show_Records:
	currentRecord = 0;
        if (records_Stored > 0) {
            ser_outf(Show_Records, "Index\tTime Stamp\tOwner ID\tRecord Data\n\r");
            proceed Display_Record;
        } else {
            ser_outf(Show_Records, "Currently no records to display\n\r");
            proceed Menu;
        }

    state Display_Record:	
        if (currentRecord < recordCount) { 
           
		ser_outf(Display_Record, "%d\t%d\t\t%d\t\t%s\n\r",
                     currentRecord,
                    (int)database[currentRecord].timeStamp,
                     database[currentRecord].ownerID,
                     database[currentRecord].recordMessage);
		
        } else {
            proceed Menu;
        }
	currentRecord++;
	proceed Display_Record;

    state Reset_Record:
    	if (recordCount >= 0){
			database[recordCount].ownerID = 0;
			database[recordCount].timeStamp = 0;
			memset(database[recordCount].recordMessage, 0, RECORD_LENGTH);
	}
	else {
		recordCount = 0;	
		records_Stored = 0;
		proceed Menu;
	}
	recordCount--;
	proceed Reset_Record;
}





