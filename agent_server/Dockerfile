FROM dart:stable AS builder
WORKDIR /app
COPY pubspec.* .
RUN dart pub get
COPY . .
RUN dart compile exe bin/server.dart -o bin/agent_server

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /app/bin/agent_server .
EXPOSE 3001
CMD ["./agent_server", "--host", "0.0.0.0", "--port", "3001"]
