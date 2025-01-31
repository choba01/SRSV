#!/usr/bin/env python
import pika
import sys

hostname = "rabbitmq"

# Poveži se s RabbitMQ serverom
import time


def connect_to_rabbitmq():
    while True:
        try:
            # Pokušaj se spojiti na RabbitMQ
            connection = pika.BlockingConnection(pika.ConnectionParameters(hostname))
            print(" [x] Uspješno spojeno na RabbitMQ")
            return connection
        except pika.exceptions.AMQPConnectionError:
            print(" [x] RabbitMQ nije dostupan, pokušavam ponovno za 5 sekundi...")
            time.sleep(1)


print("jajsdoianqwind")
# Poveži se s RabbitMQ serverom
connection = connect_to_rabbitmq()
channel = connection.channel()
print(channel)


# Stvori red za primanje poruka
channel.queue_declare(queue="process_to_receive")


# Callback funkcija za primanje poruka
def callback(ch, method, properties, body):
    print(f" [x] Primljen kvadrirani broj: {body.decode()}")
    sys.stdout.flush()


# Pretplati se na red
channel.basic_consume(
    queue="process_to_receive", on_message_callback=callback, auto_ack=True
)

print(" [*] Čekam poruke. Za izlaz pritisni CTRL+C")
channel.start_consuming()
