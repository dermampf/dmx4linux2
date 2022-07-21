_dmxconsole()
{
	case "${COMP_WORDS[COMP_CWORD-1]}" in
	    --dmx)
		COMPREPLY=( `echo /dev/dmx*` )
		return 0
		;;
	    --js)
		COMPREPLY=( `echo /dev/js* /dev/input/js*`)
		return 0
		;;
	    --mm)
		COMPREPLY=( `echo /dev/ttyS*` )
		return 0
		;;
	    --ms)
		COMPREPLY=( `echo /dev/ttyS*` )
		return 0
		;;
	    --ps2)
		COMPREPLY=( `echo /dev/input/mice /dev/input/mouse* /dev/psaux`)
		return 0
		;;
	esac

	COMPREPLY=( $( compgen -W '--dmx --fkeys --js --mm --ms --ps2 --mousescale' -- "${COMP_WORDS[COMP_CWORD]}" ) )
	return 0
}
complete -F _dmxconsole dmxconsole

_dmxtest()
{
	case "${COMP_WORDS[COMP_CWORD-1]}" in
	    --dmx)
		COMPREPLY=( "/dev/dmx" )
		return 0
		;;
	    --dmxin)
		COMPREPLY=( "/dev/dmxin" )
		return 0
		;;
	    -f)
		COMPREPLY=( `echo *` )
		return 0
		;;
	esac

	COMPREPLY=( $( compgen -W '--dmx --dmxin -r -s -f' -- "${COMP_WORDS[COMP_CWORD]}" ) )
	return 0
}
complete -F _dmxtest dmxtest

_midi2dmx()
{
	case "${COMP_WORDS[COMP_CWORD-1]}" in
	    --dmx)
		COMPREPLY=( "/dev/dmx" )
		return 0
		;;
	    --midi)
		COMPREPLY=( `/dev/midi* /dev/snd/midi*` )
		return 0
		;;
	esac

	COMPREPLY=( $( compgen -W '--dmx --midi --noteoffset --controlleroffset' -- "${COMP_WORDS[COMP_CWORD]}" ) )
	return 0
}
complete -F _midi2dmx midi2dmx

_dmxpanel()
{
	case "${COMP_WORDS[COMP_CWORD-1]}" in
	    --dmx)
		COMPREPLY=( "/dev/dmx" )
		return 0
		;;
	esac

	COMPREPLY=( $( compgen -W '--dmx --cols --rows --offset' -- "${COMP_WORDS[COMP_CWORD]}" ) )
	return 0
}
complete -F _dmxpanel dmxpanel

_dmxdisplay()
{
	case "${COMP_WORDS[COMP_CWORD-1]}" in
	    --dmx)
		COMPREPLY=( "/dev/dmx" )
		return 0
		;;
	esac

	COMPREPLY=( $( compgen -W '--dmx --cols --rows --offset --ledwidth --ledheight' -- "${COMP_WORDS[COMP_CWORD]}" ) )
	return 0
}
complete -F _dmxdisplay dmxdisplay
