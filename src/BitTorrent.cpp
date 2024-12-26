// C++ libraries
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// C libraries
#include <mpi.h>
#include <pthread.h>


using namespace std;


#define TRACKER_RANK (int) 0
#define MAX_FILES (int) 10
#define MAX_FILENAME (int) 15
#define HASH_SIZE (int) 32
#define MAX_FILE_SEGMENTS (int) 100

#define MESSAGE_LENGTH (int) 20


class FileSegment
{
 public:
    int index;
    string hash;
    vector<int> clients;


 public:
    FileSegment() {}
    FileSegment(string hashValue, vector<int> clientsValue, int indexValue)
    {
        this->hash = hashValue;
        this->clients = clientsValue;
        this->index = indexValue;
    }
};

class File
{
 public:
    string filename;
    vector<FileSegment> fileSegments;
};



class RequestedFile
{
 public:
    string requestedFilename;
    vector<FileSegment> requestedFileSegments;

    // Retine peer-urile de la care am descarcat segmentele
    // int -> indici de owneri
    vector<int> receivedFileSegments;
    int numReceivedFileSegments;

 public:
    RequestedFile()
    {
        this->receivedFileSegments.resize(MAX_FILE_SEGMENTS);
        this->requestedFileSegments.resize(MAX_FILE_SEGMENTS);

        this->numReceivedFileSegments = 0;
    }
};


class BitTorrent
{
 public:
    vector<File> filesDataBase;

    vector<RequestedFile> requestedFiles;
    int numCompletedFiles;

    vector<File> peerOwnedFiles;

    int numTasks;


 public:
    // Vor fi date ca argument functiilor MPI_Send/MPI_Recv
    enum MPI_TAGS {
        FILE_UPLOAD = 101,
        FILE_METADATA = 102,
        REQUEST_TO_TRACKER = 103,
        RESPONSE_FROM_TRACKER = 104,
        FILE_SEGMENT = 105
    };
 public:
    // enum-like class (cu string-uri)
    class MPI_MESSAGES {
     public:
        static inline string ACK = "ACK";
        static inline string COMPLETED = "COMPLETED";

        static inline string CLIENT_IS_DONE = "CLIENT IS DONE";
        static inline string REQUEST_FILE_OWNERS = "REQUEST FILE OWNERS";
        static inline string REQUEST_FILE_SEGMENT = "REQUEST FILE SEGMENT";
    };


 private:
    // Ascund constructorul default (Singleton)
    BitTorrent()
    {
        this->requestedFiles.resize(MAX_FILES);

        this->numCompletedFiles = 0;
        this->numTasks = 0;
    }
    static BitTorrent* instance;
 public:
    static BitTorrent* getInstance()
    {
        if (instance == nullptr) {
            instance = new BitTorrent();
        }
        return instance;
    }
};




BitTorrent* BitTorrent::instance = nullptr;



void requestFileSegmentsMetadataFromTracker(vector<FileSegment> &fileSegments)
{
    for (size_t i = 0; i < fileSegments.size(); i++) {
        int numOwners = fileSegments[i].clients.size();
        
        MPI_Recv(
            &numOwners, 1, MPI_INT, TRACKER_RANK,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
        
        fileSegments[i].clients.resize(numOwners);
        
        MPI_Recv(
            &fileSegments[i].index, 1, MPI_INT, TRACKER_RANK,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );

        MPI_Recv(
            fileSegments[i].clients.data(), numOwners, MPI_INT, TRACKER_RANK,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
    }
}

void requestFileMetadataFromTracker(int rank, string filename,
    vector<FileSegment> &fileSegments)
{
    // "REQUEST FILE OWNERS"
    string message(BitTorrent::MPI_MESSAGES::REQUEST_FILE_OWNERS);

    MPI_Send(
        message.data(), message.size(),
        MPI_CHAR, TRACKER_RANK,
        BitTorrent::MPI_TAGS::REQUEST_TO_TRACKER, MPI_COMM_WORLD
    );
    
    MPI_Send(
        filename.data(), filename.size(), MPI_CHAR, TRACKER_RANK,
        BitTorrent::MPI_TAGS::REQUEST_TO_TRACKER, MPI_COMM_WORLD
    );
    
    int numFileSegments = fileSegments.size();
    MPI_Recv(
        &numFileSegments, 1, MPI_INT, TRACKER_RANK,
        BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    fileSegments.resize(numFileSegments);    
    
    requestFileSegmentsMetadataFromTracker(fileSegments);
}

void addFileSegmentInRequestedFile(
    FileSegment &requestedFileSegment, RequestedFile &requestedFile,
    int selectedOwner, int fileSegmentIndex, string hash)
{
    requestedFile.receivedFileSegments.push_back(selectedOwner);
    requestedFile.numReceivedFileSegments++;
    requestedFileSegment.hash = hash;
    requestedFileSegment.index = fileSegmentIndex;
}

/*
 * descarcarea efectiva a segmentului de fisier
*/
void requestFileSegmentFromPeer(int rank, string hash, int fileIndex,
    vector<FileSegment> &fileSegments, int fileSegmentIndex)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    // Aleg primul owner al fisierului
    int selectedOwner = fileSegments[fileSegmentIndex].clients[0];


    // Trimite request la owner-ul selectat (la un peer, adica la un client),
    // pentru a obtine un segment de fisier
    // "REQUEST FILE SEGMENT"
    string message(BitTorrent::MPI_MESSAGES::REQUEST_FILE_SEGMENT);
    MPI_Send(
        message.data(), message.size(),
        MPI_CHAR, selectedOwner,
        BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD
    );
    
    MPI_Send(
        bitTorrent->requestedFiles[fileIndex].requestedFilename.data(), MAX_FILENAME,
        MPI_CHAR, selectedOwner,
        BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD
    );
    
    MPI_Send(
        &fileSegments[fileSegmentIndex].index, 1,
        MPI_INT, selectedOwner,
        BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD
    );

    // Primeste raspuns (hash-ul segmentului) de la owner-ul selectat
    char hashBuffer[HASH_SIZE + 1];
    MPI_Recv(
        hashBuffer, HASH_SIZE + 1, MPI_CHAR, selectedOwner,
        BitTorrent::MPI_TAGS::FILE_SEGMENT, MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    hash = hashBuffer;


    RequestedFile &requestedFile = bitTorrent->requestedFiles[fileIndex];
    FileSegment &requestedFileSegment =
        requestedFile.requestedFileSegments[fileSegmentIndex];
    
    addFileSegmentInRequestedFile(
        requestedFileSegment, requestedFile,
        selectedOwner, fileSegmentIndex, hash
    );
}


void writeHashInClientOutputFile(int clientIndex, string filename,
    string hash, int fileSegmentIndex)
{
    string clientFileName("client" + to_string(clientIndex) + "_" + filename);
    ofstream fout(clientFileName, ios::app);
    fout << hash << "\n";
    fout.close();
}


void manageRequestedFile(int rank, string filename, int fileIndex)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    vector<FileSegment> fileSegments;

    requestFileMetadataFromTracker(rank, filename, fileSegments);
    
    for (size_t i = 0; i < fileSegments.size(); i++) {
        string hash;
        hash.resize(HASH_SIZE);
        requestFileSegmentFromPeer(rank, hash, fileIndex, fileSegments, i);
    }

    RequestedFile &requestedFile = bitTorrent->requestedFiles[fileIndex];

    // Verific daca descarcarea fisierului a fost completata,
    // daca mai sunt segmente de primit
    if (requestedFile.numReceivedFileSegments == (int) fileSegments.size()) {
        for (int i = 0; i < requestedFile.numReceivedFileSegments; i++) {
            string hash = requestedFile.requestedFileSegments[i].hash;
            writeHashInClientOutputFile(rank, filename, hash, i);
        }
        
        bitTorrent->numCompletedFiles++;
    }
}


void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    BitTorrent *bitTorrent = BitTorrent::getInstance();
    int numRequestedFiles = (int) bitTorrent->requestedFiles.size();
    
    // Fac request-uri la tracker pana cand toate fisierele sunt descarcate
    while (bitTorrent->numCompletedFiles < numRequestedFiles) {
        for (int i = 0; i < numRequestedFiles; i++) {
            string filename = bitTorrent->requestedFiles[i].requestedFilename;
            manageRequestedFile(rank, filename, i);
        }
    }

    // Dau semnal tracker-ului ca finalizat
    // "CLIENT IS DONE"
    string message(BitTorrent::MPI_MESSAGES::CLIENT_IS_DONE);
    MPI_Send(
        message.data(), message.size(), MPI_CHAR, TRACKER_RANK,
        BitTorrent::MPI_TAGS::REQUEST_TO_TRACKER, MPI_COMM_WORLD
    );

    return NULL;
}


void requestFileSegment(MPI_Status status)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    string filename;
    filename.resize(MAX_FILENAME);
    MPI_Recv(
        filename.data(), filename.size(), MPI_CHAR, status.MPI_SOURCE,
        BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    filename.resize(strlen(filename.c_str()));

    int fileSegmentIndex = -1;
    MPI_Recv(
        &fileSegmentIndex, 1, MPI_INT, status.MPI_SOURCE,
        BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );


    // Cauta prin fisierele peer-ului (clientului)
    // si trimite hash-ul FILE_SEGMENT-ului de care e nevoie
    for (size_t i = 0; i < bitTorrent->peerOwnedFiles.size(); i++) {
        File &peerFile = bitTorrent->peerOwnedFiles[i];
        if (peerFile.filename == filename) {
            MPI_Send(
                peerFile.fileSegments[fileSegmentIndex].hash.c_str(),
                HASH_SIZE + 1,
                MPI_CHAR, status.MPI_SOURCE,
                BitTorrent::MPI_TAGS::FILE_SEGMENT, MPI_COMM_WORLD
            );
        }
    }
}


void *upload_thread_func(void *arg)
{
    while (true) {
        MPI_Status status;
        string request;
        request.resize(MESSAGE_LENGTH);

        // Primeste request-uri atat de la peer (clienti), cat si de le tracker
        MPI_Recv(
            request.data(), request.size(), MPI_CHAR, MPI_ANY_SOURCE,
            BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD, &status
        );
        request.resize(strlen(request.c_str()));


        if (request == BitTorrent::MPI_MESSAGES::COMPLETED) {
            // "COMPLETED"
            return NULL;
        } else if (request == BitTorrent::MPI_MESSAGES::REQUEST_FILE_SEGMENT) {
            // "REQUEST FILE SEGMENT"
            requestFileSegment(status);
        } else {
            continue;
        }
    }
}


void processFileSegments(File &peerFile, int peerIndex)
{
    string hash;
    hash.resize(HASH_SIZE);

    for (size_t i = 0; i < peerFile.fileSegments.size(); i++) {
        MPI_Recv(
            hash.data(), hash.size(), MPI_CHAR, peerIndex,
            BitTorrent::MPI_TAGS::FILE_METADATA,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
        hash.resize(strlen(hash.c_str()));

        // Adaug segmentul curent de fisier in structura de metadate
        peerFile.fileSegments[i]
            = FileSegment(hash, vector<int> {peerIndex}, i);
    }
}


void processPeerFiles(int numPeerOwnedFiles, int peerIndex)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    // Pentru toate fisierele detinute de un peer (client),
    // ii procesez toate segmentele
    for (int i = 0; i < numPeerOwnedFiles; i++) {
        string filename;
        filename.resize(MAX_FILENAME);
        MPI_Recv(
            filename.data(), filename.size(),
            MPI_CHAR, peerIndex,
            BitTorrent::MPI_TAGS::FILE_METADATA,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
        filename.resize(strlen(filename.c_str()));


        int numFileSegments;
        MPI_Recv(
            &numFileSegments, 1, MPI_INT, peerIndex,
            BitTorrent::MPI_TAGS::FILE_METADATA,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );

        // Completez structura care retine metadatele unui fisier
        File peerFile;
        peerFile.filename = filename;
        peerFile.fileSegments.resize(numFileSegments);

        processFileSegments(peerFile, peerIndex);
        bitTorrent->filesDataBase.push_back(peerFile);
    }
}


void insertInFilesDataBase(int numTasks, int rank)
{
    int numPeerOwnedFiles = 0;

    for (int i = 1; i < numTasks; i++) {
        // Primeste numarul de fisierele al peer-ului (clientului)
        MPI_Recv(
            &numPeerOwnedFiles, 1, MPI_INT, i,
            BitTorrent::MPI_TAGS::FILE_METADATA,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );

        // Pentru fiecare fisier al fiecarui peer (client),
        // ii procesez toate segmentele (de fisier)
        processPeerFiles(numPeerOwnedFiles, i);
    }

    // Trimit un mesaj de confirmare fiecarui peer (client)
    for (int i = 1; i < numTasks; i++) {
        // "ACK"
        string message(BitTorrent::MPI_MESSAGES::ACK);
        MPI_Send(
            message.data(), message.size(), MPI_CHAR, i,
            BitTorrent::MPI_TAGS::FILE_METADATA, MPI_COMM_WORLD
        );
    }
}

void sendCompletedMessageToAllPeers(int numTasks)
{
    // "COMPLETED"
    string message(BitTorrent::MPI_MESSAGES::COMPLETED);
    for(int i = 1; i < numTasks; i++) {
        MPI_Send(
            message.data(), message.size(), MPI_CHAR, i,
            BitTorrent::MPI_TAGS::FILE_UPLOAD, MPI_COMM_WORLD
        );
    }
}


void sendAllFileSegmentsInfo(File &file, int sourceRank)
{
    for (size_t i = 0; i < file.fileSegments.size(); i++) {
        int numOwners = file.fileSegments[i].clients.size();
        MPI_Send(
            &numOwners, 1, MPI_INT, sourceRank,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER, MPI_COMM_WORLD
        );

        MPI_Send(
            &file.fileSegments[i].index, 1, MPI_INT, sourceRank,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER, MPI_COMM_WORLD
        );


        MPI_Send(
            file.fileSegments[i].clients.data(), file.fileSegments[i].clients.size(),
            MPI_INT, sourceRank,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER, MPI_COMM_WORLD
        );
    }
}

/*
 * Functie 'helper' pentru Tracker
*/
void requestFileOwners(const MPI_Status &status)
{
    BitTorrent* bitTorrent = BitTorrent::getInstance();

    string requestedFileName;
    requestedFileName.resize(MAX_FILENAME);
    MPI_Recv(
        requestedFileName.data(), requestedFileName.size(),
        MPI_CHAR, status.MPI_SOURCE,
        BitTorrent::MPI_TAGS::REQUEST_TO_TRACKER,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    requestedFileName.resize(strlen(requestedFileName.c_str()));

    for (size_t i = 0; i < bitTorrent->filesDataBase.size(); i++) {
        if (bitTorrent->filesDataBase[i].filename != requestedFileName.c_str()) {
            continue;
        }
        
        int numFileSegments = (int) bitTorrent->filesDataBase[i].fileSegments.size();
        MPI_Send(
            &numFileSegments, 1,
            MPI_INT, status.MPI_SOURCE,
            BitTorrent::MPI_TAGS::RESPONSE_FROM_TRACKER, MPI_COMM_WORLD
        );

        sendAllFileSegmentsInfo(bitTorrent->filesDataBase[i], status.MPI_SOURCE);

        break;
    }
}



/*
 * Functie 'helper' pentru a trata semnalul "CLIENT IS DONE" primit de Tracker
*/
bool existsClientToDownload(int numTasks, int rank, vector<bool> &completedClients, const MPI_Status& status)
{
    completedClients[status.MPI_SOURCE] = true;

    // Verific daca toate peer-urile (clienti) sunt pregatiti pentru descarcare
    for (size_t i = 1; i < completedClients.size(); i++) {
        if (!completedClients[i]) {
            return false;
        }
    }

    sendCompletedMessageToAllPeers(numTasks);
    return true;
}

void tracker(int numTasks, int rank)
{
    // Primeste metadatele fisierelor de la toti peer-urile (clientii), si ii adauga in baza de date
    // Asteapta ca toate peer-urile (clienti) sa fie pregatiti pentru descarcare
    insertInFilesDataBase(numTasks, rank);

    vector<bool> completedClients(numTasks, false);

    while (true) {
        string request;
        request.resize(MESSAGE_LENGTH);
        MPI_Status status;

        // Primeste un request de la un peer (client)
        MPI_Recv(
            request.data(), request.size(), MPI_CHAR, MPI_ANY_SOURCE,
            BitTorrent::MPI_TAGS::REQUEST_TO_TRACKER, MPI_COMM_WORLD, &status
        );
        request.resize(strlen(request.c_str()));


        if (request == BitTorrent::MPI_MESSAGES::CLIENT_IS_DONE) {
            // "CLIENT IS DONE"
            bool code = existsClientToDownload(numTasks, rank, completedClients, status);

            if (code) {
                return;
            }
        } else if (request == BitTorrent::MPI_MESSAGES::REQUEST_FILE_OWNERS) {
            // "REQUEST FILE OWNERS"
            requestFileOwners(status);
        } else {
            continue;
        }
    }
}



void readFileSegments(ifstream &fin, int fileIndex)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();
    File &peerFile = bitTorrent->peerOwnedFiles[fileIndex];

    int numFileSegments;
    fin >> numFileSegments;
    peerFile.fileSegments.resize(numFileSegments);

    // Trimit tracker-ului numarul de segmente al fisierului
    MPI_Send(
        &numFileSegments, 1, MPI_INT, TRACKER_RANK,
        BitTorrent::MPI_TAGS::FILE_METADATA, MPI_COMM_WORLD
    );

    for (int i = 0; i < numFileSegments; i++) {
        string hash;
        fin >> hash;
        peerFile.fileSegments[i].hash = hash;

        // Trimit tracker-ului hash-ul segmentului de fisier
        MPI_Send(
            hash.c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK,
            BitTorrent::MPI_TAGS::FILE_METADATA, MPI_COMM_WORLD
        );
    }
}


void readOwnedFiles(ifstream &fin)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    int numPeerOwnedFiles = 0;
    fin >> numPeerOwnedFiles;

    MPI_Send(
        &numPeerOwnedFiles, 1, MPI_INT, TRACKER_RANK,
        BitTorrent::MPI_TAGS::FILE_METADATA, MPI_COMM_WORLD
    );

    bitTorrent->peerOwnedFiles.resize(numPeerOwnedFiles);

    for (int i = 0; i < numPeerOwnedFiles; i++) {
        string filename;
        fin >> filename;
        bitTorrent->peerOwnedFiles[i].filename = filename;

        // Trimit tracker-ului numele fisierului
        // (tracker-ul va retine metadate; informatii despre fisier)
        MPI_Send(
            filename.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK,
            BitTorrent::MPI_TAGS::FILE_METADATA, MPI_COMM_WORLD
        );
        

        readFileSegments(fin, i);
    }
}

void readRequestedFiles(ifstream &fin)
{
    BitTorrent *bitTorrent = BitTorrent::getInstance();

    int numRequestedFiles = 0;
    fin >> numRequestedFiles;
    bitTorrent->requestedFiles.resize(numRequestedFiles);

    for (int i = 0; i < numRequestedFiles; i++) {
        string filename;
        fin >> filename;
        bitTorrent->requestedFiles[i].requestedFilename = filename;

        for (int j = 0; j < MAX_FILE_SEGMENTS; j++) {
            bitTorrent->requestedFiles[i].receivedFileSegments[j] = -1;
        }
    }
}


/*
 * Citeste fisierul de intrare al peer-ului:
 * - numele si hash-urile pe care le detine
 * - numele fisierelor pe care vrea sa le descarce
 * si trimite Tracker-ului metadatele fisierelor pe care (peer-ul) le detine
*/
void readPeerInputFile(string inputFileName)
{
    ifstream fin(inputFileName);

    if (!fin.is_open()) {
        cerr << "[ERROR] Eroare la deschiderea fisierului de intrare." << endl;
        exit(-1);
    }

    readOwnedFiles(fin);
    readRequestedFiles(fin);

    fin.close();
}

void peer(int numTasks, int rank)
{
    // Fisierul de intrare al peer-ului (client-ului)
    string inputFileName = "in" + to_string(rank) + ".txt";
    readPeerInputFile(inputFileName);

    // Astept o confirmare din partea tracker-ului
    string response;
    response.resize(MESSAGE_LENGTH);
    MPI_Recv(
        response.data(), response.size(), MPI_CHAR, TRACKER_RANK,
        BitTorrent::MPI_TAGS::FILE_METADATA,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    response.resize(strlen(response.c_str()));

    // "ACK"
    if (response != BitTorrent::MPI_MESSAGES::ACK) {
        exit(EXIT_FAILURE);
    }

    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}


int main(int argc, char *argv[])
{
    int numTasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numTasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numTasks, rank);
    } else {
        peer(numTasks, rank);
    }

    MPI_Finalize();
}
