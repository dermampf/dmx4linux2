static void dump_dmx512_frame (const int index,
			       struct dmx512frame *frame)
{
    int i;
    if (index >= 0)
	printf ("[%d]\n", index);
    printf (" port:%d\n", frame->port);
    printf (" breaksize:%d\n", frame->breaksize);
    printf (" startcode:%d\n", frame->startcode);
    printf (" #slots:%d\n", frame->payload_size);
    printf (" data:");
    for (i=0; i < frame->payload_size && i < 25; ++i)
	printf (" %02X", frame->data[i]);
    if (i < frame->payload_size)
	printf ("...");
    printf("\n");
}
