#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <jansson.h>
#include <string.h>
#include <fcgi_stdio.h>


//gcc -o post.o post.c -lsqlite3 -ljansson  -lfcgi
//sudo apt-get install libapache2-mod-fcgid
//sudo apt-get install libfcgi-dev

typedef struct {
    char*  id;
    char* name;
    char* ticket;
}Report;


// serialize the response and send back to client-side
void create_json_array(Report info) {
    json_t *arr = json_array();
    json_t *obj = json_object();
    json_t *parent = json_object();

    char *response_data;
    
    json_object_set_new(obj, "id", json_string(info.id));
    json_object_set_new(obj, "name", json_string(info.name));
    json_object_set_new(obj, "ticket", json_string(info.ticket));

    json_array_append_new(arr, obj);

    json_object_set_new(parent, "status", json_string("success"));
    json_object_set_new(parent, "message", json_string("Successfully inserted the data.."));
    json_object_set_new(parent, "data", arr);

    response_data = json_dumps(parent,  JSON_INDENT(4)); 
   
    printf("Content-Type: application/json\n\n");    
    printf("%s", response_data);
    // use the array...
    json_decref(arr); // don't forget to free memory
}

void create_json_error_responses(const char *err_msg, const char *err_msg_add) {
    json_t *parent = json_object();
    json_object_set_new(parent, "status", json_string("error"));
    json_object_set_new(parent, "message", json_string(err_msg));     
    json_object_set_new(parent, "message_additional", json_string(err_msg_add));     
    printf("Content-Type: application/json\n\n");    
    printf("%s", json_dumps(parent,  JSON_INDENT(4)));
}


void loadJSON(Report *info) {  

    long len = strtol(getenv("CONTENT_LENGTH"), NULL, 10);
    char* incoming_data = malloc(len+1);

    json_error_t error;

    /* use for json_array_foreach */
    json_t *root;
    json_t *value;
    size_t index;

    // Read the input data from the standard input stream
    fgets(incoming_data, len + 1, stdin);

    //Parse the input data as JSON
    root = json_loads(incoming_data, 0, &error);

    if(!root) {            
        create_json_error_responses(error.text,NULL);
    }

    const char* properties[] = { "id", "name", "ticket" };
    char** infovar[] = {&info->id,&info->name,&info->ticket};
    //calculates the number of elements
    int num_properties = sizeof(properties) / sizeof(properties[0]); 
  
   // Extract the required information from the JSON data
    json_array_foreach(root, index, value) {
        const char *name_holder = json_string_value(json_object_get(value, "name"));
        const char *value_holder = json_string_value (json_object_get(value, "value")); 
        int value_holder_length = strlen(value_holder) + 1;

        for (int i = 0; i < num_properties; i++) {
                if (strcmp(name_holder, properties[i]) == 0) {
                    // re allocate db_id with equal lenght of the  source
                    *infovar[i] = (char *) malloc(value_holder_length); 
                     strncpy(*infovar[i],value_holder,value_holder_length);      
                }
       }

    } 


    free(incoming_data);

}

int main(void) {
  
  while (FCGI_Accept() >= 0) {

   Report info;
   
   sqlite3 *db;
   sqlite3_stmt *stmt;
   int dbres;

   const char *sql_statement = "insert into info values(?,?,?);";


   // function call
   // load JSON payload from client-side and deserialize
   loadJSON(&info);

      
    dbres = sqlite3_open_v2("db/data.db", &db, SQLITE_OPEN_READWRITE,NULL);

    if (dbres != SQLITE_OK) {
        create_json_error_responses(sqlite3_errmsg(db),NULL);
        sqlite3_close(db);
        return 1;
    } 

    dbres = sqlite3_prepare_v2(db, sql_statement, -1, &stmt, NULL);

    dbres = sqlite3_bind_text(stmt, 1, info.id, -1, SQLITE_TRANSIENT);
    // bitwise operator
    dbres |= sqlite3_bind_text(stmt, 2, info.name, -1, SQLITE_TRANSIENT); 
    dbres |= sqlite3_bind_text(stmt, 3, info.ticket, -1, SQLITE_TRANSIENT);
   
   
    if (dbres != SQLITE_OK) {    
        create_json_error_responses(sqlite3_errmsg(db),NULL);
        sqlite3_close(db);
        return 1;
    } else {


        char *expanded_sql = sqlite3_expanded_sql(stmt);
        char *err_msg = 0;

        dbres = sqlite3_exec(db, expanded_sql, 0, 0,&err_msg);
        
        sqlite3_finalize(stmt);

            if (dbres == SQLITE_OK) {                               
                create_json_array(info);             
                sqlite3_close(db);                
                return 1;
            } else {              
                 create_json_error_responses(sqlite3_errmsg(db),expanded_sql);
                 sqlite3_close(db);
                 return 1;

            }
    }    
   
    return 0;
 }
}


// curl -X POST -H "Content-Type: application/json" -d '[{"name": "id", "value": "2"},{"name": "name", "value": "Arnold"},{"name": "ticket", "value": "43256"}]' http://dummy.mshome.net:8081/lect/cgi-bin/midware.o


//curl -X POST -H "Content-Type: application/json" -d '[{"name": "id", "value": "2"}' http://dummy.mshome.net:8081/lect/cgi-bin/midware.o