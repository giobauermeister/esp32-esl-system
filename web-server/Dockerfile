FROM debian:bookworm-slim

# Install mosquitto and python3
RUN apt-get update && \
    apt-get install -y mosquitto python3 && \
    apt-get clean

WORKDIR /app

EXPOSE 1883 80 9001

CMD sh -c "mosquitto -c /etc/mosquitto/mosquitto.conf & python3 -m http.server 80"
