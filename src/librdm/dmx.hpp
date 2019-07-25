#pragma once
/*
 * This defined the IDmxData, IDmxTransmitter, IDmxReceiver,
 * DmxTransmitter and DmxTransceiver and the relations between them.
 *
 * You would normally derive from DmxTransceiver and implement the handle method.
 *
 * MyDmxThing : DmxTransceiver
 * {
 * public:
 *   void handle(IDmxData&);
 * };
 * void MyDmxThing::handle(const IDmxData & frame)
 * {
 *   do something with the frame and may use
 *   handleTx(txframe);
 *   to reply on it, if you are a responder.
 * }
 */

class IDmxData
{
public:
  virtual int          size() const = 0;
  virtual const void * data() const = 0;
  // can make a copy or have a reference count in the implementation.
  virtual const IDmxData & makeCopy() const { return *this; };
};

class IDmxReceiver
{
public:
  virtual void handle(const IDmxData&) = 0;
};

class IDmxTransmitter
{
public:
  virtual void registerReceiver(IDmxReceiver&) = 0;
};


class IDmxTransceiver : public IDmxTransmitter, public IDmxReceiver
{
};


class DmxTransceiver : public IDmxTransceiver
{
private:
  IDmxReceiver * m_receiver;
public:
  virtual void registerReceiver(IDmxReceiver & r)
  {
    // may throw an exception if m_receiver is allready bound.
    m_receiver = &r;
  }
protected:
  void handleTx(const IDmxData & f)
  {
    if (m_receiver)
      m_receiver->handle(f);
  }
};

void dump(const void * _data, const unsigned int size);
void dump(const IDmxData & f);
