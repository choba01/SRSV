import pika
import random
import time
import sys
from datetime import datetime

hostname = "rabbitmq"


def connect_to_rabbitmq():
    while True:
        try:
            connection = pika.BlockingConnection(pika.ConnectionParameters(hostname))
            print(" [x] Uspješno spojeno na RabbitMQ")
            return connection
        except pika.exceptions.AMQPConnectionError:
            print(" [x] RabbitMQ nije dostupan, pokušavam ponovno za 1 sekundu...")
            time.sleep(1)


def wait_for_queues(channel):
    while True:
        try:
            # Provjeri postoji li red "send_to_process"
            channel.queue_declare(queue="send_to_process", passive=True)
            print(" [x] Red 'send_to_process' je dostupan")
            break
        except pika.exceptions.ChannelClosedByBroker:
            print(" [x] Red 'send_to_process' nije dostupan, čekam...")
            time.sleep(1)


# Poveži se s RabbitMQ
connection = connect_to_rabbitmq()
channel = connection.channel()

# Pričekaj da redovi budu dostupni
wait_for_queues(channel)

# Kontinuirano šalji nasumične brojeve
try:
    start_time = time.time()  # Zabilježi vrijeme početka
    while True:
        number = random.randint(1, 10)
        channel.basic_publish(
            exchange="", routing_key="send_to_process", body=str(number)
        )
        print(f" [x] Sent: {number}")
        sys.stdout.flush()

        if time.time() - start_time > 10:  # Nakon 10 sekundi
            print(f" [x] Namjerno izazivam grešku... Vrijeme: {datetime.now()}")
            sys.stdout.flush()
            sys.exit(1)  # Izlaz s greškom

        time.sleep(5)
except KeyboardInterrupt:
    print(" [x] Prekinuto slanje.")
finally:
    connection.close()
