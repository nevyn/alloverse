#include <string.h>
#include <erl_interface.h>
#include <allonet/allonet.h>
#include "erl_comm.h"
#include <enet/enet.h>
#include <sys/fcntl.h>

 #define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

void erl_free_term_handle(ETERM **term) { erl_free_term(*term); }
void erl_free_compound_handle(ETERM **term) { erl_free_compound(*term); }
#define scoped_term __attribute__ ((__cleanup__(erl_free_term_handle)))
#define scoped_comp __attribute__ ((__cleanup__(erl_free_compound_handle)))
void free_handle(uint8_t **handle) { free(*handle); }
#define scoped __attribute__ ((__cleanup__(free_handle)))

void write_term(ETERM *term)
{
    scoped uint8_t *outbuf = malloc(erl_term_len(term));
    erl_encode(term, outbuf);
    write_cmd(outbuf, erl_term_len(term));
}

alloserver *serv;

void handle_erl()
{
    printf("hello handling erl\n");
    scoped uint8_t *buf = read_cmd();
    if(!buf)
        return;
    scoped_comp ETERM *tuplep = erl_decode(buf);
    printf("done decoding\n");
    
    printf("printing term\n");
    int wtf = erl_print_term(tuplep, stdout);
    printf("done printing %d\n", wtf);

    
    scoped_comp ETERM *command = erl_element(1, tuplep);
    printf("command is %s\n", ERL_ATOM_PTR(command));
    
    scoped_comp ETERM *reqId = erl_element(2, tuplep);
    scoped_comp ETERM *args = erl_element(3, tuplep);
    
    if (strcmp(ERL_ATOM_PTR(command), "disconnect") == 0) {
        scoped_term ETERM* e_client_ptr = erl_element(1, args);
        void *client_ptr = (void*)ERL_LL_UVALUE(e_client_ptr);
        alloserver_client *client;
        LIST_FOREACH(client, &serv->clients, pointers)
        {
            if(client == client_ptr) {
                serv->disconnect(serv, client);
                scoped_comp ETERM *msg = erl_format("{response, ~w, ok}", reqId);
                write_term(msg);
                return;
            }
        }
        scoped_comp ETERM *msg = erl_format("{response, ~w, {error, \"no such client\"}}", reqId);
        write_term(msg);
        return;
    } else if(strcmp(ERL_ATOM_PTR(command), "ping") == 0) {
        scoped_comp ETERM *msg = erl_format("{response, ~w, statepong}", reqId);
        write_term(msg);
        return;
    }

    
    scoped_comp ETERM *msg = erl_format("{response, ~w, {error, \"no such command\"}}", reqId);
    write_term(msg);
    return;
}

int main()
{
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allostate");
        return -1;
    }

    erl_init(NULL, 0);

    if(fcntl(erlin, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to set erlin as non-blocking");
        return -4;
    }
    
    printf("allostateport open as %d\n", getpid());
    
    while (1) {
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, erlin);
        
        int selectr = enet_socketset_select(MAX(0, erlin), &set, NULL, 100);
        if(selectr < 0) {
            perror("select failed, terminating");
            return -3;
        } else if(ENET_SOCKETSET_CHECK(set, erlin)) {
            handle_erl();
        }
    }
    return 0;
}