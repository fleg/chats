'use strict';

const net = require('node:net');

const MAX_MESSAGE_LENGTH = 64;

const clients = new Map();

const addClient = (clientId, client) => {
  clients.set(clientId, client);
};

const removeClient = (clientId) => {
  clients.delete(clientId);
};

const broadcast = (authorId, msg) => {
  clients.forEach((client, id) => {
    if (id === authorId) return;

    client.socket.write(msg);
  });
};

const print = (msg) => process.stdout.write(msg);
const eprint = (msg) => process.stderr.write(msg);

const clientHandler = (socket) => {
  const { remoteAddress, remotePort } = socket;
  const clientId = `${remoteAddress}:${remotePort}`;

  addClient(clientId, { socket });

  print(`[INFO] New connection ${clientId}\n`);

  socket.setEncoding('ascii');
  socket.on('error', (err) => {
    eprint(`[ERROR] Unexpected client ${clientId} error: ${err.stack}\n`);
    socket.destroy();
    removeClient(clientId);
  });

  socket.on('close', () => {
    eprint(`[INFO] Client ${clientId} disconnected\n`);
    removeClient(clientId);
  });

  socket.on('data', (msg) => {
    if (msg.length > MAX_MESSAGE_LENGTH) {
      socket.destroy();
    } else {
      print(`[INFO] Message from ${clientId}: ${msg}`);
      broadcast(clientId, msg);
    }
  });
};

const main = () => {
  const port = 9000;
  const host = '0.0.0.0';
  const server = net.createServer(clientHandler);

  server.on('error', (err) => {
    eprint(`[ERROR] Unexpected server error: ${err.stack}\n`);
    process.exit(1);
  });

  server.on('listening', () => {
    print(`[INFO] Listening on ${host}:${port}\n`);
  });

  server.listen(port, host);
};

main();
