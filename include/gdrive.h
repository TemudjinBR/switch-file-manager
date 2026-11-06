// Módulo de rede + Google Drive (HTTPS pelo serviço SSL nativo do Switch)
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "cloud/download_manager.h"

namespace gd {

struct Item {
    std::string id, name, mime;
    uint64_t size = 0;
    bool dir = false;
    bool gdoc = false;  // arquivo nativo do Google (Docs, Planilhas...) - não baixável
};

// Recebe os bytes enviados/recebidos. Retorne false para cancelar.
struct Progress {
    virtual bool tick(uint64_t bytes) = 0;
    virtual downloads::Action action() { return downloads::Action::Continue; }
    virtual void transfer(const downloads::Status&) {}
    virtual ~Progress() {}
};

bool netInit();
void netExit();
std::string lastError();
enum class ErrorSource { None, Storage, Google, Network, Protocol, Other };
ErrorSource lastErrorSource();
enum class ConnectionState { Connected, Disconnected, AuthorizationRequired, InvalidAuthorization, NetworkError, StorageError };
ConnectionState connectionState();
std::string connectionLabel();
std::string accountName();

// ---- login
bool isLoggedIn();
bool hasCredentials();
bool loadSavedLogin();   // tenta usar o login salvo (renova o token)
bool reconnectSavedLogin();
bool keepConnected();
bool setKeepConnected(bool keep);
bool autoLoginAvailable();
bool forgetAccount(); // somente após confirmação explícita na UI; não revoga no Google
bool logout();           // bloqueia login automático, preservando token/credencial

// Servidor local (porta 8080) onde o usuário cola os dados pelo navegador do celular.
bool loginServerStart(std::string& urlForUser);
// 0 = aguardando, 1 = login concluído, -1 = erro (veja lastError(); o servidor continua ativo)
int loginServerPoll();
void loginServerStop();

// ---- Drive
bool listFolder(const std::string& folderId, std::vector<Item>& out);
bool downloadFile(const Item& it, const std::string& dstPath, Progress* pr);
bool pendingDownloads(std::vector<downloads::Job>& jobs, unsigned& invalid);
bool resumeDownload(const downloads::Job& job, Progress* progress);
bool cancelDownload(const downloads::Job& job);
bool uploadFile(const std::string& srcPath, const std::string& parentId, const std::string& name, Progress* pr);
bool createFolder(const std::string& parentId, const std::string& name, std::string& newId);
bool renameItem(const std::string& id, const std::string& newName);
bool trashItem(const std::string& id);

#ifdef HOST_TEST
extern int testPort;           // porta do servidor de teste
extern std::string testDir;    // pasta que substitui sdmc:/switch/switch-manager
extern int testLoginPort;
#endif

}  // namespace gd
