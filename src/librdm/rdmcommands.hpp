// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <functional>

namespace Rdm { class RdmApi; };
class RdmInterpreter
{
private:
  Rdm::RdmApi  & m_rdm;
  std::vector<std::string> m_topCommands;
public:
    typedef std::vector<std::string> Arguments;
    typedef std::function<bool(std::ostream & out, const Arguments &)> CommandFunction;
    typedef std::function<void(const std::string &,CommandFunction)> IteratorFunction;

    RdmInterpreter(Rdm::RdmApi & _rdm);
    ~RdmInterpreter();

    void foreachFunction(IteratorFunction c);

    bool mute(std::ostream & out, const Arguments & args);
    bool unmute(std::ostream & out, const Arguments & args);
    bool discovery(std::ostream & out, const Arguments & args);
    bool listdevices(std::ostream & out, const Arguments & args);
    bool set(std::ostream & out, const Arguments & args);
    bool get(std::ostream & out, const Arguments & args);
    bool test(std::ostream & out, const Arguments & args);
};
