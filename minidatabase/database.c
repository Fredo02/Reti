#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "database.h"

Database* create_database(IndexNodeString* name, IndexNodeString* surname, IndexNodeString* address, IndexNodeInt* age) {
    Database *database = malloc(sizeof(Database));
    if (database == NULL) {
        return NULL;
    } 
    
    database->name = name;
    database->surname = surname;
    database->address = address;
    database->age = age;

    return database;
}

IndexNodeString* create_tree_string(char* value, Persona* persona) {
    IndexNodeString *root = malloc(sizeof(IndexNodeString));
    if (root == NULL) {
        return NULL;
    } 
    
    root->value = value;
    root->left = NULL;
    root->right = NULL;
    root->persona = persona;

    return root;
}

IndexNodeInt* create_tree_int(int value, Persona* persona) {
    IndexNodeInt *root = malloc(sizeof(IndexNodeInt));
    if (root == NULL) {
        return NULL;
    } 
    
    root->value = value;
    root->left = NULL;
    root->right = NULL;
    root->persona = persona;

    return root;
}

void free_database(Database* database){
    free(database->name);
    free(database->surname);
    free(database->address);
    free(database->age);
    free(database);
}

void insert_string(IndexNodeString* node, char* value, Persona* persona){
    if(node == NULL) return;
    
    if(strcmp(value, node->value) <= 0){
        if(node->left == NULL){
            IndexNodeString* left = create_tree_string(value, persona);
            node->left = left;
            return;
        }
        insert_string(node->left, value, persona);
        return;
    }

    if (node->right == NULL) {
        IndexNodeString* right = create_tree_string(value, persona);
        node->right = right;
        return;
    }
    insert_string(node->right, value, persona);
}

void insert_int(IndexNodeInt* node, int value, Persona* persona){
    if(node == NULL) return;
    
    if(value <= node->value){
        if(node->left == NULL){
            IndexNodeInt* left = create_tree_int(value, persona);
            node->left = left;
            return;
        }
        insert_int(node->left, value, persona);
        return;
    }

    if (node->right == NULL) {
        IndexNodeInt* right = create_tree_int(value, persona);
        node->right = right;
        return;
    }
    insert_int(node->right, value, persona);
}

void insert(Database * database, Persona * persona){
    if (database == NULL) {
        return;
    }
    char* name, *surname, *address;
    int age;
    strncpy(name, persona->name, strlen(persona->name));
    name[strlen(persona->name)] = '\0';
    strncpy(surname, persona->surname, strlen(persona->surname));
    surname[strlen(persona->surname)] = '\0';
    strncpy(address, persona->address, strlen(persona->address));
    address[strlen(persona->address)] = '\0';
    age = persona->age;

    insert_string(database->name, name, persona);
    insert_string(database->surname, surname, persona);
    insert_string(database->address, address, persona);
    insert_int(database->age, age, persona);
}

Persona* findByString(IndexNodeString* node, char* value){
    if(node == NULL) return;

    if(strcmp(value, node->value) == 0) return node->persona;
    else if(strcmp(value, node->value) < 0) findByString(node->left, value);
    else if(strcmp(value, node->value) > 0) findByString(node->right, value);
}

Persona* findByName(Database * database, char * name){
    return findByString(database->name, name);
}

Persona* findBySurname(Database * database, char * surname){
    return findByString(database->surname, surname);
}

Persona* findByAddress(Database * database, char * address){
    return findByString(database->address, address);
}

Persona* findByInt(IndexNodeInt* node, int value){
    if(node == NULL) return;

    if(value == node->value) return node->persona;
    else if(value < node->value) findByInt(node->left, value);
    else if(value > node->value) findByInt(node->right, value);
}

Persona* findByAge(Database * database, int age){
    return findByInt(database->age, age);
}