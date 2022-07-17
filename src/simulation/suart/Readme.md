# suart

This is an abstract uart which is based on a transmit and a receive fifo.
All communication is serialized thru these two queues, even changing the baudrate. That means that a change baudrate command is executed only after all commands
before it are executed. On the receive side this is the same.

For it's clients it removes the complexity of handling with special cases due to the diffrent uart implementations with different features like different fifo sizes or fifo feaures and different interrupts and maybe something like RS485 support.
