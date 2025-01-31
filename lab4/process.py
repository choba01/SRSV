import pika
import time
import sys

hostname = "rabbitmq"


def connect_to_rabbitmq():
    while True:
        try:
            # Pokušaj se spojiti na RabbitMQ
            connection = pika.BlockingConnection(pika.ConnectionParameters(hostname))
            print(" [x] Uspješno spojeno na RabbitMQ")
            return connection
        except pika.exceptions.AMQPConnectionError:
            print(" [x] RabbitMQ nije dostupan, pokušavam ponovno za 1 sekundi...")
            time.sleep(1)


# Poveži se s RabbitMQ serverom
connection = connect_to_rabbitmq()
channel = connection.channel()

# Stvori redove za primanje i slanje poruka
channel.queue_declare(queue="send_to_process")  # Red za primanje poruka
channel.queue_declare(queue="process_to_receive")  # Red za slanje odgovora


# Callback funkcija za obradu poruka
def callback(ch, method, properties, body):
    # Pretvori poruku u broj
    number = int(body.decode())
    print(f" [x] Primljen broj: {number}")
    sys.stdout.flush()

    # Kvadriraj broj
    squared_number = number**2

    # Pošalji kvadrirani broj u drugi red
    channel.basic_publish(
        exchange="", routing_key="process_to_receive", body=str(squared_number)
    )
    print(f" [x] Poslan kvadrirani broj: {squared_number}")
    sys.stdout.flush()


# Pretplati se na red za primanje poruka
channel.basic_consume(
    queue="send_to_process", on_message_callback=callback, auto_ack=True
)

print(" [*] Čekam poruke. Za izlaz pritisni CTRL+C")
channel.start_consuming()
