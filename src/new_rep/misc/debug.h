            for(int i = index; i < messageEndOffset; i++){
                printf("%02x", ((unsigned char)(q->buffer[i])) );
            //printf("%2X", ((unsigned char *) q->buffer)[i] );
            }
