#include <poll.h>
#include <stdio.h>
//#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>

#include <readline/readline.h>
#include <readline/history.h>

#include "commandinterpreter.hpp"
#include "rdmcommands.hpp"

#include <dmx.hpp>
#include "dmx4linux2.hpp"
#include "dmx4linux2interface.hpp"
#include "rdmapi.hpp"

#define CONFIG_USE_DMX4LINUX2

#ifdef CONFIG_USE_DMX4LINUX2
class Dmx4Linux2Commands
{
  Dmx4Linux2Device & m_dmxdevice;
public:
  Dmx4Linux2Commands(Dmx4Linux2Device & _dmxdevice)
    : m_dmxdevice(_dmxdevice)
  {
  }
  bool opendmx(std::ostream & out, const CommandParser::Arguments & args);
};

bool Dmx4Linux2Commands::opendmx(std::ostream & out, const CommandParser::Arguments & args)
{
    if (args.size() > 1)
    {
	if (m_dmxdevice.open(args[1].c_str()))
	{
	    out << "opened device " << args[1] << std::endl;
	    return true;
	}
	out << "failed to open device "
	    << args[1] << " : "
	    << strerror(errno) << std::endl;
    }
    return false;
}
#endif


static std::function<std::vector<std::string>(const std::string &,
					      const std::string &)> g_listMatchings;

static char* completion_generator(const char* text, int state)
{
#if 0
    std::cout << "completion_generator \"" << text << "\""
	      << " state:" << state
	      << std::endl;
#endif
  
    // This function is called with state=0 the first time; subsequent calls are
    // with a nonzero state. state=0 can be used to perform one-time
    // initialization for this completion session.
    static std::vector<std::string> matches;
    static size_t match_index = 0;

    if (state == 0)
    {
	// During initialization, compute the actual matches for 'text' and keep
	// them in a static vector.
	matches.clear();
	match_index = 0;
#if 0
	std::cout << std::endl
		  << "completion_generator \"" << text << "\""
		  << " rl_line_buffer:" << rl_line_buffer
		  << std::endl;
#endif
	matches = g_listMatchings(std::string(rl_line_buffer),
				  std::string(text));
    }

    if (match_index < matches.size())
    {
	return strdup(matches[match_index++].c_str());
    }

    // We return nullptr to notify the caller no more matches are available.
    return nullptr;
}

static char ** rdm_completer(const char* text, int start, int end)
{
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, completion_generator);
}


int main(int argc, char ** argv)
{
    //=== RDM Initialization ===
    const char * historyFilename = "rdmsh_history.txt";
    std::string lastCommand;
    CommandParser  rdmParser(std::cout);
    Rdm::RdmApi    rdmApi;
    RdmInterpreter rdmInterpreter(rdmApi);

    g_listMatchings = std::bind(&CommandParser::listMatchings,
				&rdmParser,
				std::placeholders::_1,
				std::placeholders::_2);

    rdmInterpreter.foreachFunction
	(
	    [&rdmParser](const std::string & name, RdmInterpreter::CommandFunction func)
	    {
		rdmParser.addCommand(name,func);
	    }
	);
    rl_attempted_completion_function = rdm_completer;


#ifdef CONFIG_USE_DMX4LINUX2
    Dmx4Linux2Device    dmxdevice;
    Dmx4Linux2Interface dmxinterface(dmxdevice);
    bool run = false;
    std::thread dmxDeviceThread ([&dmxdevice, &dmxinterface, &run]()
				 {
				   run = true;
				   while(run)
				     {
				       if (dmxdevice)
					 dmxinterface.handleIngressData();
				       else
					 sleep(1);
				     }
				 }
				 );

    Dmx4Linux2Commands  dmxCommands(dmxdevice);
    rdmParser.addCommand("opendmx", std::bind(&Dmx4Linux2Commands::opendmx,
					      &dmxCommands,
					      std::placeholders::_1,
					      std::placeholders::_2));
#endif

    dmxinterface.registerReceiver(rdmApi);
    rdmApi.registerReceiver(dmxinterface);

    //-- execute commands from the commandline.
    if (argc > 1)
	for (int i = 1; i < argc; ++i)
	{
	    if (std::string(argv[i]) == "--exit")
	    {
		sleep(1);
		exit(0);
	    }
	    else
	    {
		printf ("execute-arg: '%s'\n", argv[i]);
		rdmParser.Execute(argv[i]);
	    }
	}

    if (read_history (historyFilename))
	printf ("failed to read command history from %s\n", historyFilename);
    else
	printf ("read command history from %s\n", historyFilename);

    char * cmd = 0;
    while ((cmd = readline("rdm> ")) != 0)
    {
	if (rdmParser.Execute(cmd) && lastCommand != cmd)
	{
	    add_history(cmd);
	    lastCommand = cmd;
	}
	free(cmd);
	cmd = 0;
    }

#ifdef CONFIG_USE_DMX4LINUX2
    run = false;
    std::cout << "waiting for dmx thread" << std::endl;
    dmxDeviceThread.join();
#else
#endif

    if (write_history (historyFilename))
	printf ("failed to write command history to %s\n", historyFilename);
    else
	printf ("wrote command history to %s\n", historyFilename);
    return 0;
}
