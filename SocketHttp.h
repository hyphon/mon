#ifndef SOCKETHTTP_H
#define SOCKETHTTP_H

#define HTTP_HEADER_LEN 1024
#define HTTP_LEN 4096
#define HTTP_NEWLINE "\r\n"
#define KEY_LEN 1024

#define NODE_3DES_KEY          node_3des_key
#define NODE_3DES_IV           node_3des_iv

#define DEFAULT_NODE_3DES_KEY  "t^^BvGfAdUTixobQP$HhsOsD"
#define DEFAULT_NODE_3DES_IV   "=V#s%CS)"

extern char node_3des_key[KEY_LEN];
extern char node_3des_iv[KEY_LEN];

extern int isOpen(char * ip, short port, int type);

#endif
