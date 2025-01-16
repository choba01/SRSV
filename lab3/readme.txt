korišten makro za pozivanje svih funkcija vezanih uz postavljanje prioriteta i kreiranje dretvi.
dretvama ulazima pridružen max prioritet (99), dok je upravljačima pridružen prioritet max - id -1, kako bi osigurao da bude sigurno manje od 99.
tako će dretve manjeg perioda imati veći prioritet od jer imaju manji id.
određivanje broja iteracija potrebnih za potrošit 10 ms je napravljen prema pseudokodu iz uputa za lab.