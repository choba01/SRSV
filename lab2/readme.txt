Struktura koda za drugi lab:
kod se nalazi unutar jedne datoteke, koja se sastoji od maina, pomoćnih funkcija za vrijeme preuzetih iz skripte i profesorova github repozitorija (https://github.com/ljelenkovic/srsv-primjeri-koda), funkcije koju imaju dretve simulatori te funkciju koja periodički glumi upravljača.
Kod je pisan po uputama, uz nadograđivanje prvog labosa, iz kojeg je uzeta funkcionalnost dretvi te potrebne strukture podataka.
Problem koji mi se javlja je da mi neke dretve iako se još nisu pojavile, nakon prekida simulacije sa zakašnjenjem promijene stanje.
Ideja za upravljač je bila imati strukturu koja mi prati broj perioda, postavlja trenutno aktivnu dretvu, čijom usporedbom sa zahtjevajucaDretva varijablom može raspoznati ako se neka dretva jos obrađuje, pa po potrebi daje drugu periodu, uz provjeru ako je moguće istu dodijeliti s obzirom na uvjete zadatka.
Dretve prate svoja stanja i potrebne podatke kroz strukturu ulazniParametri.