# Tema 2 APD | BitTorrent

> Am testat codul sursa folosind **Docker**.


## Server descentralizat

Tema consta in implementarea unui server descentralizat,
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

> Fara o cheie de decriptare, nimeni nu ar putea intelege
> ce scrie in fisierele de intrare ale clientilor in[0-9].txt.
> Sunt curios ce ati ascuns acolo in spatele acelor **hash**-uri :).

In timpul descarcarii de fisiere,
descarcarea nu se face traditional, de la un server,
ci de la un alt client care detine fisierul de interes.
Practic, fiecare client se comporta pe post de "server",
de baza de date de fisiere.
Aici vine "tracker"-ul in actiune, un Middle-Man intre clienti,
care stie cine ce fisiere are, fara ca el sa aibe vreun fisier,
el redirectioneaza traficul de la un client la altul.


## Tracker

Tracker-ul nu are niciun fisier fizic,
dar el stie informatiile referitoare la aceste fisiere.
Cu alte cuvinte, tracker-ul retine metadate despre aceste fisiere;
practic date despre date (date despre fisiere), printre care:
- Numele fisierelor
- O lista inlantuita de segmente ale fisierului, si are urmatoarea structura
  - Hash-ul segmentului
  - Indexul segmentului in fisier
  - Clientii care detin acest segment


> Tracker-ul nu detine fisierele,
> dar stie hash-urile lor.


### Initializare Tracker

La inceputul conexiunii (formarii topologiei),
tracker-ul trebuie sa stie pentru fiecare client toate aceste metadate,
ce fisiere au peer-urile, care sunt segmentele lor.
De aceea, la inceput, tracker-ul
va primi toate aceste informatii de la fiecare client, urmand
sa trimita cate un **"ACK"** la fiecare client dupa ce a primit metadatele.

La nivel de cod, acest flux de executie este reprezentat de
functia `insertInFilesDataBase()`, apelata fix la inceputul functiei `tracker()`.
Functia este responsabila si de trimiterea **"ACK"**-urilor.

### Interactiunea cu Tracker-ul

Tracker-ul ruleaza intr-o bucla,
oprita la primirea mesajului **"CLIENT IS DONE"**
in MPI din partea tuturor clientilor.
Atunci cand tracker-ul primeste acest mesaj specific de la un client,
marcheaza ca descarcarea dorita de acesta s-a incheiat.
In acest moment, tracker-ul verifica daca
si ceilalti clienti si-au completat fisierele sau nu.
Daca mai exista cel putin un client care mai are de descarcat,
tracker-ul continua sa ruleze, altfel se opreste,
trimitand cate un mesaj **"COMPLETED"** fiecarui peer
(metoda `sendCompletedMessageToAllPeers()`),
care anunta ca nimeni nu mai are nimic de facut.

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


### String-uri in MPI pentru Tracker

- Trimise de tracker (fiecarui peer/client)
  - **ACK**: se trimite la initierea topologiei,
    dupa ca tracker-ul a obtinut metadatele tuturor fisierelor
    ("semn ca poate incepe comunicatia intre clienti
    pentru a se descarca fisierele dorite")
  - **COMPLETED**: la final de tot,
    dupa ce toate descarcarile au luat sfarsit
    (toti clientii au primit ce doreau sa descare)
- Primite de tracker
  - **CLIENT IS DONE**:
    Unul dintre peer-uri (clienti) si-a descarcat fisierele dorite.
    (Daca nu mai exista niciun alt client cu fisiere in curs de descarcare,
    tracker-ul va trimite **"COMPLETED"** in MPI catre toate peer-urile)
  - **REQUEST FILE OWNERS**:
    la primirea acestui mesaj, alaturi de numele unui fisier,
    tracker-ul va apela metoda `requestFileOwners`,
    si va raspunde clientului cu metadatele fisierului cautat



## Client (peer)

Asa cum am discutat mai devreme,
client-ul atat detine fisiere, cat si doreste sa descare,
de aceea fiecare client este de tip **peer**,
conexiunea contextul acestei teme fiind de tipul **peer-to-peer** (**P2P**),
avand tracker-ul ca intermediar.


Primul lucru pe care il fac pentru un peer (client)
este sa ii citesc fisierul de intrare
(fisier care contine info despre fisiere :))
in metoda `readPeerInputFile()` (si restul metodelor asociate acesteia).
Acest fisier de intrare contine informatii despre:
- Fisisierele pe care clientul le detine
- Fisierele pe care clientul vrea sa le descarce

In privinta fisierelor detinute de client,
pe parcursul citirii, informez tracker-ul ce hash-uri are acesta.

Dupa ce s-a finalizat citirea,
astept un raspuns din partea tracker-ului,
care ma astept sa fie **"ACK"**,
altfel opresc fortat programul si nu mai fac nimic.

> **ACK** = confirmare din partea tracker-ului.

Dupa primirea **"ACK"**-ului din partea tracker-ului
(lucru care atesta ca toti clientii i-au furnizat metdatale tuturor fisierelor),
client-ul porneste cate un thread pentru download si updload.


### Thread-ul de download

Cat timp nu am descarcat atatea fisiere cate si-a propus clientul,
descarc segmentele aferente lipsa (metoda `manageRequestedFile()`).
Ce fac eu este sa ii cer mai intai tracker-ului sa imi spuna
indicii segmentelor de care am nevoie si cine le detine practic,
ca mai apoi sa descarc segmentele de fisiere
(de fapt **hash**-urile efective - metoda `requestFileSegmentFromPeer()`) de la ei,
si sa scriu toate aceste date in fisierul de output al clientului care face descarcarea.

> Tracker-ul "redirecteaza" clientul catre alte peer-uri
> (care au ceea ce clientul vrea sa descarce).

La final, cand clientul si-a primit toate fisierele,
ii semnalez tracker-ului acest lucru,
trimitand mesajul **"CLIENT IS DONE"** in MPI.


### Thread-ul de upload

Intr-o bucla care se opreste
doar in momentul primirii mesajului **"COMPLETED"** de la tracker,
astept sa primesc request-uri din partea clientilor.

Imi dau seama ca am primit un request in momentul in care
am primit mesajul **"REQUEST FILE SEGMENT"**,
request care mai asteapta din partea celuilalt client
atat numele fisierului pe care celalalt vrea sa il descarce,
cat si indexul segmentului dorit.
Peer-ul curent va putea astfel sti cu ce **hash** sa raspunda.

> - Request: "REQUEST FILE SEGMENT" + nume fisier + indice segment de fisier
> - Response: un hash

Aceasta logica este implementata la nivel de cod in functia `requestFileSegment`.


## Alte detalii strict de implementare

Pentru usurinta mea, am ales sa rezolv aceasta tema in C++,
datorita claselor si unor structuri de date deja implementate in **std**,
dar, mergand pe principiul ca *"totul in viata se plateste"*,
am avut niste batai de cap cand a trebuit sa trimit mesaje din C++ in MPI:
la vectori si string-uri, a trebuit sa imi dau seama ca MPI asteapta
un pointer la datele respective...dar, la apeluri de receive pe string-uri,
a trebuit sa dau le redimensionez in functie de sirul aferent in C
...ca altfel nu se face comparatia cum trebuie intre string-uri :(. 


