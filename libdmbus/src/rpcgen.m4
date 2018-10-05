define(`foreach',`ifelse(eval($#>2),1,`pushdef(`$1',`$3')$2`'popdef(`$1')`'ifelse(eval($#>3),1,`$0(`$1',`$2',shift(shift(shift($@))))')')')
define(`list', `ifelse($#,2,$2,$2$1`foreach($1,shift(shift($@)))')')dnl list(separator,e1,e2,e3)
define(`capitalize', `translit($1, `a-z', `A-Z')')
define(`comma',`,')

define(MSGSTRUCT, `struct msg_$1
{
    struct dmbus_msg_hdr hdr;
foreach(`X',`    X;
',shift($@))} DMBUS_PACKED $1;')


define(`MSG_STRUCTS',`')
define(`SERV_MSG_OPS',`')
define(`SERV_MSG_HANDLERS',`')
define(`DM_RPC_FUNCS',`')
define(`DM_RPC_DEFS',`')


define(`DEFINE_MESSAGE', `define(`MSGID_'$2,DMBUS_MSG_`'capitalize($2))'dnl
                         `define(`MSG_STRUCTS',MSG_STRUCTS
`#define' MSGID_$2 $1
MSGSTRUCT(shift($@)))'dnl
)

define(`DEFINE_IN_RPC_NO_RETURN', `define(`SERV_MSG_OPS', SERV_MSG_OPS`'dnl
void (*$1)(``void *priv, struct msg_$1 *msg, size_t msglen'');
)'dnl
`define(`SERV_MSG_HANDLERS', SERV_MSG_HANDLERS`'dnl
case MSGID_$1:
{
        size_t len = m->hdr.msg_len;

        if (c->rpc_ops->$1) {
            c->rpc_ops->$1(``c->priv, &m->''$1``, len'');
        }
        break;
}
)'dnl
)

define(`DEFINE_IN_RPC_WITH_RETURN', `define(`SERV_MSG_OPS', SERV_MSG_OPS`'dnl
int (*$1)(``void *priv, struct msg_$1 *msg, size_t msglen, struct msg_$2 *out'');
)'dnl
`define(`SERV_MSG_HANDLERS', SERV_MSG_HANDLERS`'dnl
case MSGID_$1:
{
        struct msg_$2 out;
        size_t len = m->hdr.msg_len;
        int ret = -1;

        if (c->rpc_ops->$1) {
            ret = c->rpc_ops->$1(``c->priv, &m->''$1``, len, &out'');
        }
        out.hdr.return_value = (uint32_t) ret;
        send_msg(``c, ''MSGID_$2``, &out, sizeof (out)'');
        break;
}
)'dnl
)

define(`DEFINE_OUT_RPC', `define(`DM_RPC_DEFS', DM_RPC_DEFS
`void '$1`(dmbus_client_t client, struct msg_'$1` *msg, size_t msglen);')'dnl
                        `define(`DM_RPC_FUNCS', DM_RPC_FUNCS
`void '$1`(dmbus_client_t client, struct msg_'$1` *msg, size_t msglen)'
{
    struct dmbus_client *c = client;

    send_msg(c, MSGID_$1, msg, msglen);
}
)'dnl
)

define(`DEFINE_BROADCAST_RPC', `define(`DM_RPC_DEFS', DM_RPC_DEFS
`void '$1`(struct msg_'$1` *msg, size_t msglen);')'dnl
                        `define(`DM_RPC_FUNCS', DM_RPC_FUNCS
`void '$1`(struct msg_'$1` *msg, size_t msglen)'
{
    broadcast_msg(MSGID_$1, msg, msglen);
}
)'dnl
)

define(`INTERFACE_HASH',dnl
`# define DMBUS_SHA1_STRING "'substr(include(dmbus.sha1), 0, 40)`"'dnl
)
