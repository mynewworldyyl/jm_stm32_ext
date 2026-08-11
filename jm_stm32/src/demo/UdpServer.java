import java.io.IOException;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class UdpServer {
    public static void main(String[] args) {
        int port = 9999;
        if (args.length > 0) {
            port = Integer.parseInt(args[0]);
        }
        DatagramSocket[] socketWrapper = new DatagramSocket[1];
        try {
            socketWrapper[0] = new DatagramSocket(port);
            final DatagramSocket socket = socketWrapper[0];
            System.out.println("UDP Server started on port " + port);
            byte[] buffer = new byte[1024];
            final InetAddress[] lastClientAddress = new InetAddress[1];
            final int[] lastClientPort = new int[1];
            ScheduledExecutorService heartbeatScheduler = Executors.newSingleThreadScheduledExecutor();
            heartbeatScheduler.scheduleAtFixedRate(() -> {
                try {
                    if (lastClientAddress[0] != null && lastClientPort[0] > 0) {
                        String timestamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date());
                        String heartbeatText = "[heartbeat] " + timestamp;
                        byte[] heartbeatData = heartbeatText.getBytes("UTF-8");
                        DatagramPacket heartbeatPacket = new DatagramPacket(heartbeatData, heartbeatData.length, lastClientAddress[0], lastClientPort[0]);
                        socket.send(heartbeatPacket);
                        System.out.println("Heartbeat sent: " + heartbeatText);
                    }
                } catch (Exception e) {
                    System.err.println("Heartbeat failed: " + e.getMessage());
                }
            }, 5, 5, TimeUnit.SECONDS);
            while (true) {
                DatagramPacket request = new DatagramPacket(buffer, buffer.length);
                socket.receive(request);
                String received = new String(request.getData(), 0, request.getLength(), "UTF-8").trim();
                lastClientAddress[0] = request.getAddress();
                lastClientPort[0] = request.getPort();
                String timestamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date());
                String responseText = "[" + timestamp + "] " + received;
                byte[] responseData = responseText.getBytes("UTF-8");
                DatagramPacket response = new DatagramPacket(responseData, responseData.length, request.getAddress(), request.getPort());
                socket.send(response);
                System.out.println("Received: " + received + " -> Responded: " + responseText);
            }
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (socketWrapper[0] != null && !socketWrapper[0].isClosed()) {
                socketWrapper[0].close();
            }
        }
    }
}
