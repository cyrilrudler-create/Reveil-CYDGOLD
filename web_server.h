#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>

// On dit au reste du projet que 'server' est défini ailleurs
extern WebServer server;

// Prototype de la fonction qui va configurer toutes les routes (/, /update, /delete)
void setup_web_server();

#endif