#include "../basic/basic.h"

#define FRAGMENT_SIZE 10
#define FRAGMENT_BIT (1<<31)

bitstream_t bstream;
endTimer_t sendTimer;

int duration = 200;

void netcon_init()
{
    bstream.buf = zidmalloc(GENERALZONE, MAX_MSGLEN);
    stream_init(&bstream, bstream.buf, MAX_MSGLEN);
}

void netcon_setup(netcon_t *con)
{
    memset(con, 0, sizeof(netcon_t));
    con->incomingSequence = 0;
    con->outgoingSequence = 1;
    con->windowStartSequence = 0;
}

netcon_packetstate_e netcon_getPacketState(netcon_t *con, int sequence)
{

    return con->sentPacketStates[sequence & (SENDWINDOW_SIZE-1)];
}

qbool netcon_shouldSend(netcon_t *con)
{
    if((con->outgoingSequence - con->windowStartSequence + 1) == SENDWINDOW_SIZE)
    {
        return qfalse;
    }

    return qtrue;
    // if(checkTimer(&sendTimer) && con->sendState == NETCON_READY)
    //     return qtrue;
    // return qfalse;
}

int netcon_transmitFragment(netcon_t *con)
{
    int ret, headcurb, payloadLen;
    stream_init(&bstream, bstream.buf, bstream.bufsize);
    
    stream_writeInt(&bstream, con->sendFragSequence);
    stream_writeByte(&bstream, 1);
    stream_writeByte(&bstream, con->sendFragID);
    headcurb = bstream.curbyte;

    stream_writeData(&bstream, con->sendFragBuffer, con->sendFragLength);
    
    ret = net_sendPacket(&con->remoteAddress, &bstream);

    if(ret == -1)
    {
        com_printf("ERROR: failed to send packet \n");
        return -1;
    }

    payloadLen = ret - headcurb;
    
    if(payloadLen < con->sendFragLength)
    {
        con->sendFragLength -= payloadLen;
        zmemcpy(con->sendFragBuffer, con->sendFragBuffer + payloadLen, con->sendFragLength);
        con->sendFragID++;

    } else if(payloadLen == con->sendFragLength)
    {
        con->sendState = NETCON_READY;
        con->sentPacketStates[(con->sendFragSequence - 1) & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_SENT;
    }

    return 0;
}

int netcon_transmit(netcon_t * con, int length, byte *data)
{
    int ret = 0;
    int headcurb;
    int payloadLen;
    int currentOutgoingSequence;
    
    if(length > bstream.bufsize)
    {
        com_printf("ERROR: payload length too high %d %d\n", length, bstream.bufsize);
        return -1;
    }


    stream_init(&bstream, bstream.buf, bstream.bufsize);

    
    currentOutgoingSequence = con->outgoingSequence;

    stream_writeInt(&bstream, currentOutgoingSequence);
    stream_writeByte(&bstream, 0);
    stream_writeInt(&bstream, length);
    stream_writeInt(&bstream, con->incomingSequence);
    headcurb = bstream.curbyte;

    stream_writeData(&bstream, data, length);

    con->outgoingSequence++;
    con->sentPacketStates[(con->outgoingSequence) & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_READY;


    ret = net_sendPacket(&con->remoteAddress, &bstream);
    payloadLen = ret - headcurb;
    if(ret == -1)
    {
        com_printf("ERROR: failed to send packet \n");
        return -1;
    }
    if(payloadLen < length)
    {
        con->sendFragID = 1;
        con->sendState = NETCON_FRAGMENT;
        zmemcpy(con->sendFragBuffer, data + payloadLen, length - payloadLen);
        con->sendFragLength = length - payloadLen;
        con->sendFullFragLength = length;
        con->sendFragSequence = currentOutgoingSequence;
        con->sentPacketStates[(currentOutgoingSequence) & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_FRAGMENTED;
    }
    else
    {
        con->sentPacketStates[(currentOutgoingSequence) & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_SENT;
    }


    startTimer(&sendTimer, duration);
    return 0;
}

int netcon_processFragment(netcon_t *con, bitstream_t *bs, int incomingSequence)
{
    int payloadLen;
    int headcurb;

    // check if expecting unsentfragments
    if(con->recvState == NETCON_FRAGMENT)
    {
        // check if this fragment packet is part of the current set
        if(con->recvFragSequence != incomingSequence)
        {
            com_printf("Did not get the expected fragment sequence\n");
            return -1;
        }
    } else {
        com_printf("Current receiving connection state is not fragment\n");
        return -1;
    }

    // check if no fragment packet is skipped
    int fragID = stream_readByte(bs);

    if(fragID != con->recvFragID + 1)
    {
        com_printf("Received fragment packet has skipped fragID. received=%d, processed=%d \n", fragID, con->recvFragID);
        return -1;
    }

    headcurb = bs->curbyte;

    con->recvFragID = fragID;
    payloadLen = bs->datalen - headcurb;

    // check if the received data is beyond the length given
    if((con->recvFragLength + payloadLen) > con->recvFullFragLength)
    {
        com_printf("Addition of received fragment packet exceeds total packet length. exceeded size=%d, full packet size=%d\n"
        ,con->recvFragLength + payloadLen, con->recvFullFragLength );
        return -1;
    }

    // copy the data into the fragment buffer
    zmemcpy(con->recvFragBuffer + con->recvFragLength, bs->buf + headcurb, payloadLen);
    con->recvFragLength += payloadLen;

    // check if the full packet length has arrived
    if(con->recvFullFragLength == con->recvFragLength)
    {
        stream_init(bs, bs->buf, bs->bufsize);
        zmemcpy(bs->buf, con->recvFragBuffer, con->recvFullFragLength);

        con->recvState = NETCON_READY;
    }
    return 0;
}

int netcon_process(netcon_t *con, bitstream_t *bs)
{
    int i, incomingSequence, fullPayloadLen, retval = 0;
    int isFragmented = 0;
    int payloadLen;
    int headcurb;
    int ackSeq;

    incomingSequence = stream_readInt(bs);
    isFragmented = stream_readByte(bs);


    // check if packet is fragmented
    if(isFragmented)
    {
        retval = netcon_processFragment(con, bs, incomingSequence);
    } else {
        if(con->recvState == NETCON_FRAGMENT)
        {
            con->recvState = NETCON_READY;
            con->recvFragLength = 0;
            con->recvFullFragLength = 0;
        }
        else {
            con->incomingSequence = incomingSequence;

            fullPayloadLen = stream_readInt(bs);
            ackSeq = stream_readInt(bs);


            con->lastAckSequence = ackSeq;

            headcurb = bs->curbyte;
            payloadLen = bs->datalen - headcurb;

            if(fullPayloadLen > payloadLen)
            {
                con->recvState = NETCON_FRAGMENT;

                con->recvFullFragLength = fullPayloadLen;
                con->recvFragLength = payloadLen;
                con->recvFragSequence = incomingSequence;
                con->recvFragID = 0;

                zmemcpy(con->recvFragBuffer, bs->buf + headcurb, payloadLen); 
            }
        }
    }

    if(ackSeq < con->windowStartSequence)
    {
        printf("ack is too old \n");
        return -1;
    }

    // printf("acked seq %d \n", ackSeq);

    for(int i = con->windowStartSequence; (i != ackSeq 
        && (i < (con->windowStartSequence + SENDWINDOW_SIZE))); i++)
    {
        con->sentPacketStates[i & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_DROPPED;
    }

    con->sentPacketStates[ackSeq & (SENDWINDOW_SIZE - 1)] = NETCON_PACKET_SUCCESS;
    con->windowStartSequence = ackSeq;


    return retval;
}