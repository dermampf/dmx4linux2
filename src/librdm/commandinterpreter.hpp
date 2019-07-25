#pragma once

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <vector>
#include <map>
#include <string>
#include <fstream>

class CommandParser
{
public:
    typedef std::vector<std::string> Arguments;
    typedef std::function<bool(std::ostream & out, const Arguments &)> CommandFunction;
    typedef std::map<std::string,CommandFunction> CommandMap;
private:
    CommandMap m_functions;
    std::ostream & m_out;
public:
    CommandParser(std::ostream & _out)
	: m_out(_out)
	{
	    using namespace std::placeholders;
	    addCommand("execfile", std::bind(&CommandParser::execute_file, this, _1, _2));
	}

    void addCommand(const std::string & _name, CommandParser::CommandFunction _f)
	{
	    m_functions[_name] = _f;
	}

    std::vector<std::string> commandNames() const
	{
	    std::vector<std::string> keys;
	    for (auto i : m_functions)
		keys.push_back(i.first);
	    return keys;
	}

    std::vector<std::string> listMatchings(
		      const std::string & cmdlineSoFar,
		      const std::string & text)
	{
	    // Collect a vector of matches: vocabulary words that begin with text.
	    std::vector<std::string> matches;
	    for (auto word : commandNames())
	    {
		if (word.size() >= text.size() &&
		    word.compare(0, text.size(), text) == 0)
		{
		    matches.push_back(word);
		}
	    }
	    return matches;
	}

    bool Execute(const char * _command)
	{
	    Arguments args;
	    boost::split(args,
			 _command,
			 boost::is_any_of(" \t"),
			 boost::token_compress_on);
	    return Execute(args);
	}

    bool Execute(const Arguments & args)
	{
	    if (args.size() <= 0)
		return false;

	    CommandMap::const_iterator i = m_functions.find(args[0]);
	    if (i != m_functions.end())
	    {
		i->second(m_out, args);
		return true;
	    }
	    else for (auto i : args)
		 {
		     printf ("  '%s'\n", i.c_str());
		 }
	    return false;
	}

    /*
     * open a file and execute all commands from that file.
     */
    bool execute_file(std::ostream & out, const Arguments & args)
	{
	    if (args.size() > 1)
	    {
		std::ifstream infile(args[1]);
		if (infile)
		{
		    std::string line;
		    while (std::getline(infile, line))
		    {
			if (line[0]!='#')
			    Execute(line.c_str());
		    }
		}
	    }
	    return true;
	}
};

