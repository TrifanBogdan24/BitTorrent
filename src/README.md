# Trifan Bogdan Cristian - 311 CD

## Tema 2 APD | BitTorrent

### Server descentralizat

Tema constanta in implementarea unui tracker descentralizat,
care are la baza un tracker, in jurul caruia avem niste clienti.

Se precizeaza in enunt ca un client poate avea unul dintre urmatoarele 3 roluri:
- seed -> se ocupa doar cu upload-ul segmentelor de fisier
- leecher -> se ocupa doar cu descarcarea de fisiere
- **peer** -> se ocupa atat cu incarcarea, cat si cu descarcarea de fisiere

De vreme ce in contextul temei,
fiecare client atat detine fisiere, cat si descarca fisiere,
pot spune ca toti clientii sunt de tip **peer**.


Atunci cand descarcam un fisier (also si in viata reala),
noi practic descarcam pe rand segmente din acel fisier,
primind fisierul bucata cu bucata,
care mai apoi circula criptata in internet.
Acest lucru l-am implementat si in cadrul temei:
fiecare fisier reprezinta de fapt o lista de hash-uri.

> Fara o cheie de decriptare, nimeni nu ar putea ce scrie in fisierele de intrare in[0-9].txt.
> Sunt curios ce ati pus acolo :).

In timpul descarcarii de fisiere,
descarcarea nu se face traditional, de la un server,
ci de la un alt client care detine fisierul de interes.
Practic, fiecare client se comporta pe post de "server",
de baza de date de fisiere.
Aici vine "tracker"-ul in actiune, un Middle-Man intre clienti,
care stie cine ce fisiere are, fara ca el sa aibe vreun fisier,
el redirectioneaza traficul de la un client la altul.


### Tracker

Tracker-ul nu are niciun fisier fizic,
dar el stie informatiile referitoare la aceste fisiere.
Cu alte cuvinte, tracker-ul retine metadate despre aceste fisiere;
practic date despre date (fisiere), printre care:
- Numele fisierelor
- O lista inlantuita de segmente ale fisierului, si are urmatoarea structura
  - Hash-ul segmentului
  - Indexul segmentului in fisier
  - Clientii care detin acest segment 


#### Initializare Tracker

La inceputul conexiunii (formarii topologiei),
tracker-ul trebuie sa stie pentru fiecare client toate aceste metadate,
ce fisiere au peer-urile, care sunt segmentele lor.
De aceea, la inceput, tracker-ul
va primit toate aceste informatii de la fiecare client, urmand
sa trimita cate un **"ACK"** la fiecare client dupa ce a primit metadatele.

La nivel de cod, acest flux de executie este reprezentat de
functia `insertInFilesDataBase()`, apelata fix la inceputul functiei `tracker()`.
Functia trimite si **"ACK"**-urile.

#### Interactiunea cu Tracker-ul

Tracker-ul ruleaza intr-o bucla infinita,
oprita la primirea mesajului **"CLIENT IS DONE"**
in MPI din partea tuturor clientilor.
Atunci cand tracker-ul primeste acest mesaj specific de la un client,
marcheaza ca descarcarea dorita de acesta s-a incheiat.
In acest moment, tracker-ul verifica daca
si ceilalti clienti si-au completat fisierele sau nu.
Daca mai exista cel putin un client care mai are de descarcat,
tracker-ul continua sa ruleze, altfel se opreste,
trimitand cate un mesaj **"COMPLETED"** fiecarui peer
(metoda `sendCompletedMessageToAllPeers()`).

In timpul executiei tracker-ului in bucla "infinita"
(oprita atunci cand toti clientii au terminat de descarcat...
asa cum am mentionat precedent),
acesta poate primi request-uri de tipul **"REQUEST FILE OWNER"**
pentru un fisier, metoda `requestFileOwners()`.
Pentru acest request, tracker-ul are nevoie de (primeste) numele fisierului dorit,
si returneaza clientului care a facut request-ul urmatoarele informatii:
- Numarul de segmente al fisierului
- Pentru fiecare segment, in metoda `sendAllFileSegmentsInfo()`:
  - Numarul de clienti care detin acel segment
  - Lista clientilor care detin acel segment
  - Indexul segmentului in fisier


#### String-uri in MPI pentru Tracker

- Trimise de tracker (fiecarui peer/client)
  - **ACK**:
  - **COMPLETED**:
- Primite de tracker
  - **CLIENT IS DONE**:
  - **REQUEST FILE OWNERS**: 



### Client (peer)

Asa cum am discutat mai devreme,
client-ul atat detine fisiere, cat si doreste sa descare,
de aceea fiecare cleint este de tip **peer**,
conexiunea contextul acestei teme fiind de tipul **peer-to-peer** (**P2P**),
avand tracker-ul ca intermediar.










