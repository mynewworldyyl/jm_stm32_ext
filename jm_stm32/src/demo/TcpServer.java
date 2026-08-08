import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

public class TcpServer {
    private static final int PORT = 8888;
    private static final int READ_TIMEOUT_MS = 60 * 60 * 1000;
    private static final int KEEPALIVE_INTERVAL_MS = 15_000;
    private static final int MAX_IDLE_MS = 60 * 60 * 1000;
    private static final int PUSH_INTERVAL_MS = 10_000;
    private static final String PUSH_PREFIX = "[PUSH] server heartbeat ";
    private static final AtomicLong PUSH_COUNT = new AtomicLong(0);

    public static void main(String[] args) {
        int port = PORT;
        if (args.length > 0) {
            port = Integer.parseInt(args[0]);
        }
        ExecutorService pool = Executors.newCachedThreadPool(r -> {
            Thread t = new Thread(r, "tcp-worker-" + (tCount.incrementAndGet()));
            t.setDaemon(true);
            return t;
        });
        ScheduledExecutorService pusher = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "tcp-pusher");
            t.setDaemon(true);
            return t;
        });
        ConcurrentHashMap<String, Socket> clients = new ConcurrentHashMap<>();
        ServerSocket serverSocket = null;
        try {
            serverSocket = new ServerSocket(port);
            serverSocket.setReuseAddress(true);
            System.out.println("TCP Server started on port " + port);
            pusher.scheduleAtFixedRate(() -> pushToAll(clients), PUSH_INTERVAL_MS, PUSH_INTERVAL_MS, TimeUnit.MILLISECONDS);
            while (true) {
                Socket clientSocket = serverSocket.accept();
                String clientId = clientSocket.getRemoteSocketAddress().toString();
                clients.put(clientId, clientSocket);
                pool.execute(() -> handleClient(clientSocket, clients, clientId));
            }
        } catch (IOException e) {
            System.err.println("Server error: " + e.getMessage());
            e.printStackTrace();
        } finally {
            pusher.shutdownNow();
            pool.shutdownNow();
            closeAllClients(clients);
            if (serverSocket != null && !serverSocket.isClosed()) {
                try { serverSocket.close(); } catch (IOException e) { e.printStackTrace(); }
            }
        }
    }

    private static final ThreadLocal<SimpleDateFormat> DATE_FMT =
            ThreadLocal.withInitial(() -> new SimpleDateFormat("yyyy-MM-dd HH:mm:ss"));

    private static final AtomicInteger tCount = new AtomicInteger(0);

    private static void handleClient(Socket socket, ConcurrentHashMap<String, Socket> clients, String clientId) {
        System.out.println("[" + clientId + "] Connected");
        InputStream in = null;
        BufferedWriter writer = null;
        try {
            socket.setTcpNoDelay(true);
            socket.setKeepAlive(true);
            socket.setSoTimeout(READ_TIMEOUT_MS);
            socket.setSendBufferSize(16 * 1024);

            in = socket.getInputStream();
            writer = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), "UTF-8"));

            byte[] buf = new byte[4096];
            int read;
            while ((read = in.read(buf)) != -1) {
                String timestamp = DATE_FMT.get().format(new Date());
                String hex = bytesToHex(buf, 0, read);
                String text = bytesToText(buf, 0, read);
                String response = "[" + timestamp + "] recv " + read + " bytes [" + text + "] " + hex;
                writer.write(response);
                writer.newLine();
                writer.flush();
                System.out.println("[" + clientId + "] Received " + read + " bytes [" + text + "] " + hex);
            }
            System.out.println("[" + clientId + "] Read returned -1, peer closed normally");
        } catch (SocketTimeoutException e) {
            System.out.println("[" + clientId + "] Read timeout after " + READ_TIMEOUT_MS + "ms, no incoming data, still alive");
        } catch (IOException e) {
            System.err.println("[" + clientId + "] IO error: " + e.getClass().getSimpleName() + " - " + e.getMessage());
        } finally {
            clients.remove(clientId);
            closeQuietly(clientId, in);
            closeQuietly(clientId, writer);
            closeQuietly(clientId, socket);
        }
    }

    private static String bytesToHex(byte[] bytes, int off, int len) {
        StringBuilder sb = new StringBuilder(len * 3);
        for (int i = off; i < off + len; i++) {
            if (i > off) sb.append(' ');
            sb.append(String.format("%02X", bytes[i]));
        }
        return sb.toString();
    }

    private static String bytesToText(byte[] bytes, int off, int len) {
        StringBuilder sb = new StringBuilder(len);
        for (int i = off; i < off + len; i++) {
            char c = (char)(bytes[i] & 0xFF);
            if (c >= 32 && c <= 126) {
                sb.append(c);
            } else {
                sb.append('.');
            }
        }
        return sb.toString();
    }

    private static void pushToAll(ConcurrentHashMap<String, Socket> clients) {
        long seq = PUSH_COUNT.incrementAndGet();
        if(clients.size() <= 0) return;
        String msg = PUSH_PREFIX + "seq=" + seq + " time=" + DATE_FMT.get().format(new Date());
        System.out.println("[PUSHER] Broadcasting to " + clients.size() + " clients: " + msg);
        for (Iterator<Socket> it = clients.values().iterator(); it.hasNext(); ) {
            Socket s = it.next();
            if (s == null || s.isClosed() || !s.isConnected()) {
                it.remove();
                continue;
            }
            BufferedWriter w = null;
            try {
                w = new BufferedWriter(new OutputStreamWriter(s.getOutputStream(), "UTF-8"));
                w.write(msg);
                w.newLine();
                w.flush();
            } catch (IOException e) {
                System.err.println("[PUSHER] Failed to push to " + s.getRemoteSocketAddress() + ": " + e.getMessage());
                it.remove();
            } finally {
                if (w != null) {
                    try { w.flush(); } catch (IOException e) { /* ignore */ }
                }
            }
        }
    }

    private static void closeAllClients(ConcurrentHashMap<String, Socket> clients) {
        for (Socket s : clients.values()) {
            if (s != null && !s.isClosed()) {
                try { s.close(); } catch (IOException e) { /* ignore */ }
            }
        }
        clients.clear();
    }

    private static void closeQuietly(String clientId, Closeable c) {
        if (c == null) return;
        try { c.close(); } catch (IOException e) { /* ignore */ }
    }
}
