APPLICATION="./obj/$(hostname)/t_dmxrtuart"
if [ -f "$APPLICATION" ] ; then
    chrt -r 99 $APPLICATION pl011 /dev/uio0 $*
fi
