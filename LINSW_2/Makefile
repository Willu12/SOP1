# That script compiles the worms application for the target
# platform, if the BR/output/host/environment-setup has been
# sourced to the current shell session.
OBJS := memo.c
worms: $(OBJS)
	$(CC) -o memo $(CFLAGS) $(LDFLAGS) $(OBJS) -l periphery
$(OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@
