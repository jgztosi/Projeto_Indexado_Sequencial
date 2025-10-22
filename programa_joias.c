#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define MAX_STR 30
#define INDICE_INTERVALO 200

typedef struct {
    unsigned int registro_id;
    uint64_t product_id;
    uint64_t category_id;
    char category_alias[MAX_STR];
    char brand[MAX_STR];
    double price_usd;
    char gender[MAX_STR];
} Joia;

typedef struct {
    uint64_t chave;
    long     posicao;
} Indice;

// ==================== Funções auxiliares ====================

static void preencherEspacos(char *dest, const char *orig) {
    strncpy(dest, orig ? orig : "", MAX_STR - 1);
    dest[MAX_STR - 1] = '\0';
    size_t len = strlen(dest);
    for (size_t i = len; i < MAX_STR - 1; i++) dest[i] = ' ';
    dest[MAX_STR - 1] = '\0';
}

static void rstrip_inplace(char *s) {
    int i = (int)strlen(s) - 1;
    while (i >= 0 && (s[i] == ' ' || s[i] == '\r' || s[i] == '\n' || s[i] == '\0')) {
        s[i] = '\0';
        i--;
    }
}

static long contar_registros(const char *dados_path) {
    FILE *f = fopen(dados_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fclose(f);
    if (tam < 0) return 0;
    return tam / (long)sizeof(Joia);
}

// garante que joias.del exista e tenha 1 byte por registro
static int ensure_del_file(const char *dados_path, const char *del_path) {
    long nreg = contar_registros(dados_path);

    FILE *fl = fopen(del_path, "rb");
    if (fl) {
        fseek(fl, 0, SEEK_END);
        long t = ftell(fl);
        fclose(fl);
        if (t == nreg) return 1;
    }
    // (re)cria do tamanho certo, tudo ativo (0)
    fl = fopen(del_path, "wb");
    if (!fl) return 0;
    unsigned char zero = 0;
    for (long i = 0; i < nreg; i++) fwrite(&zero, 1, 1, fl);
    fclose(fl);
    return 1;
}

static int get_del_flag(FILE *fdel, long recno, unsigned char *flag) {
    if (fseek(fdel, recno, SEEK_SET) != 0) return 0;
    return fread(flag, 1, 1, fdel) == 1;
}

static int set_del_flag(FILE *fdel, long recno, unsigned char flag) {
    if (fseek(fdel, recno, SEEK_SET) != 0) return 0;
    return fwrite(&flag, 1, 1, fdel) == 1;
}

static void print_joia(const Joia *j) {
    char alias[MAX_STR], brand[MAX_STR], gender[MAX_STR];
    memcpy(alias, j->category_alias, MAX_STR); alias[MAX_STR-1] = '\0'; rstrip_inplace(alias);
    memcpy(brand, j->brand, MAX_STR); brand[MAX_STR-1] = '\0'; rstrip_inplace(brand);
    memcpy(gender, j->gender, MAX_STR); gender[MAX_STR-1] = '\0'; rstrip_inplace(gender);

    printf("Registro ID: %u\n", j->registro_id);
    printf("Produto ID: %" PRIu64 "\n", j->product_id);
    printf("Categoria: %" PRIu64 "\n", j->category_id);
    printf("Alias: %s\n", alias);
    printf("Marca: %s\n", brand);
    printf("Preco (USD): %.2f\n", j->price_usd);
    printf("Genero: %s\n", gender);
    printf("------------------------------------\n");
}

// ==================== Índice ====================

static int compararIndice(const void *a, const void *b) {
    const Indice *ia = (const Indice*)a;
    const Indice *ib = (const Indice*)b;
    if (ia->chave < ib->chave) return -1;
    if (ia->chave > ib->chave) return 1;
    return 0;
}

static int rebuild_index(const char *dados_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dados_path, del_path)) {
        printf("Erro ao preparar joias.del\n");
        return 0;
    }

    FILE *fd = fopen(dados_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    FILE *fi = fopen(idx_path, "wb");
    if (!fd || !fl || !fi) {
        if (fd) fclose(fd);
        if (fl) fclose(fl);
        if (fi) fclose(fi);
        printf("Erro abrindo arquivos para reconstruir indice.\n");
        return 0;
    }

    Joia j;
    unsigned char flag;
    long recno = 0;
    long validos = 0;
    long escritos = 0;

    while (fread(&j, sizeof(Joia), 1, fd) == 1) {
        if (!get_del_flag(fl, recno, &flag)) { fclose(fd); fclose(fl); fclose(fi); return 0; }
        if (flag == 0) {
            if (validos % INDICE_INTERVALO == 0) {
                Indice ix = { (uint64_t)j.registro_id, recno * (long)sizeof(Joia) };
                fwrite(&ix, sizeof(Indice), 1, fi);
                escritos++;
            }
            validos++;
        }
        recno++;
    }

    fclose(fd); fclose(fl); fclose(fi);
    return 1;
}

static Indice* carregarIndice(const char *idx_path, int *qtd) {
    FILE *fi = fopen(idx_path, "rb");
    if (!fi) { *qtd = 0; return NULL; }
    fseek(fi, 0, SEEK_END);
    long tam = ftell(fi);
    rewind(fi);
    int n = (int)(tam / (long)sizeof(Indice));
    if (n <= 0) { fclose(fi); *qtd = 0; return NULL; }
    Indice *vet = (Indice*)malloc(n * sizeof(Indice));
    if (!vet) { fclose(fi); *qtd = 0; return NULL; }
    fread(vet, sizeof(Indice), n, fi);
    fclose(fi);
    qsort(vet, n, sizeof(Indice), compararIndice);
    *qtd = n;
    return vet;
}

static long buscaBinariaIndice(Indice *vet, int tamanho, uint64_t chave) {
    int i = 0, f = tamanho - 1;
    while (i <= f) {
        int m = (i + f) / 2;
        if (vet[m].chave == chave) return vet[m].posicao;
        if (vet[m].chave < chave) i = m + 1; else f = m - 1;
    }
    if (i == 0) return 0;
    if (i >= tamanho) return vet[tamanho-1].posicao;
    return vet[i-1].posicao;
}

// varre sequencialmente a partir de 'offset_inicio' até achar registro_id == chave
static long scan_por_chave(const char *dados_path, const char *del_path,
                           long offset_inicio, unsigned int chave, long *out_recno) {
    FILE *fd = fopen(dados_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    if (!fd || !fl) { if (fd) fclose(fd); if (fl) fclose(fl); return -1; }

    if (fseek(fd, offset_inicio, SEEK_SET) != 0) { fclose(fd); fclose(fl); return -1; }

    long recno = offset_inicio / (long)sizeof(Joia);
    Joia j; unsigned char flag;

    while (fread(&j, sizeof(Joia), 1, fd) == 1) {
        if (!get_del_flag(fl, recno, &flag)) { fclose(fd); fclose(fl); return -1; }
        if (flag == 0) {
            if (j.registro_id == chave) {
                long pos = recno * (long)sizeof(Joia);
                if (out_recno) *out_recno = recno;
                fclose(fd); fclose(fl);
                return pos;
            }
            if (j.registro_id > chave) break; // arquivo ordenado por registro_id
        }
        recno++;
    }

    fclose(fd); fclose(fl);
    return -1;
}

// ------------------------ operações ------------------------

static void mostrarPrimeirosRegistros(const char *dados_path, const char *del_path, int limite) {
    if (!ensure_del_file(dados_path, del_path)) {
        printf("Erro ao preparar joias.del\n"); return;
    }
    FILE *fd = fopen(dados_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    if (!fd || !fl) { if (fd) fclose(fd); if (fl) fclose(fl); printf("Erro ao abrir arquivos.\n"); return; }

    Joia j; unsigned char flag;
    int mostrados = 0;
    long recno = 0;

    while (mostrados < limite && fread(&j, sizeof(Joia), 1, fd) == 1) {
        if (!get_del_flag(fl, recno, &flag)) break;
        if (flag == 0) { print_joia(&j); mostrados++; }
        recno++;
    }

    fclose(fd); fclose(fl);
    printf("Exibidos %d registros.\n", mostrados);
}

static void consultarPorChave(const char *dados_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dados_path, del_path)) {
        printf("Erro ao preparar joias.del\n"); return;
    }

    // garante índice
    FILE *t = fopen(idx_path, "rb");
    if (!t) {
        if (!rebuild_index(dados_path, del_path, idx_path)) return;
    } else fclose(t);

    int qtd = 0;
    Indice *indice = carregarIndice(idx_path, &qtd);
    if (!indice || qtd == 0) { printf("Falha ao carregar indice.\n"); if (indice) free(indice); return; }

    unsigned int chave;
    printf("Digite o Registro ID: ");
#if defined(_WIN32)
    if (scanf("%u", &chave) != 1) { printf("Entrada invalida.\n"); free(indice); return; }
#else
    if (scanf("%u", &chave) != 1) { printf("Entrada invalida.\n"); free(indice); return; }
#endif

    long off0 = buscaBinariaIndice(indice, qtd, (uint64_t)chave);
    long recno = -1;
    long pos = scan_por_chave(dados_path, del_path, off0, chave, &recno);
    if (pos < 0) {
        printf("Registro nao encontrado.\n");
        free(indice);
        return;
    }

    FILE *fd = fopen(dados_path, "rb");
    if (!fd) { printf("Erro ao abrir dados.\n"); free(indice); return; }
    Joia j;
    fseek(fd, pos, SEEK_SET);
    fread(&j, sizeof(Joia), 1, fd);
    fclose(fd);

    printf("\nRegistro encontrado:\n");
    printf("------------------------------------\n");
    print_joia(&j);

    free(indice);
}

// Insere no fim com novo registro_id (último + 1) mantendo ordenação (sequencial por ID crescente).
static void inserirJoiaOrdenada(const char *dados_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dados_path, del_path)) { printf("Erro ao preparar joias.del\n"); return; }

    Joia novo;
    // calcula próximo registro_id
    long nreg = contar_registros(dados_path);
    unsigned int prox_id = (unsigned int)(nreg + 1);
    novo.registro_id = prox_id;

    // coleta dados (exceto registro_id)
    printf("Produto ID: ");
#if defined(_WIN32)
    if (scanf("%llu", &novo.product_id) != 1) { printf("Entrada invalida.\n"); return; }
#else
    if (scanf("%" SCNu64, &novo.product_id) != 1) { printf("Entrada invalida.\n"); return; }
#endif

    printf("Categoria ID: ");
#if defined(_WIN32)
    if (scanf("%llu", &novo.category_id) != 1) { printf("Entrada invalida.\n"); return; }
#else
    if (scanf("%" SCNu64, &novo.category_id) != 1) { printf("Entrada invalida.\n"); return; }
#endif

    printf("Alias: ");
    int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) strcpy(buf, "");
    buf[strcspn(buf, "\r\n")] = '\0';
    preencherEspacos(novo.category_alias, buf);

    printf("Marca: ");
    if (!fgets(buf, sizeof(buf), stdin)) strcpy(buf, "");
    buf[strcspn(buf, "\r\n")] = '\0';
    preencherEspacos(novo.brand, buf);

    printf("Preco (USD): ");
    if (scanf("%lf", &novo.price_usd) != 1) novo.price_usd = 0.0;

    printf("Genero: ");
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    if (!fgets(buf, sizeof(buf), stdin)) strcpy(buf, "");
    buf[strcspn(buf, "\r\n")] = '\0';
    preencherEspacos(novo.gender, buf);

    // grava no fim do arquivo (mantém ordem por registro_id)
    FILE *fd = fopen(dados_path, "ab");
    if (!fd) { printf("Erro ao abrir %s\n", dados_path); return; }
    fwrite(&novo, sizeof(Joia), 1, fd);
    fclose(fd);

    // acrescenta flag 0 (ativo) no .del
    FILE *fdel = fopen(del_path, "ab");
    if (!fdel) { printf("Erro ao abrir %s\n", del_path); return; }
    unsigned char zero = 0;
    fwrite(&zero, 1, 1, fdel);
    fclose(fdel);

    // reconstruir índice (para considerar novo válido e manter saltos consistentes)
    rebuild_index(dados_path, del_path, idx_path);
    printf("Inserido com Registro ID = %u\n", prox_id);
}

static void removerJoia(const char *dados_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dados_path, del_path)) { printf("Erro ao preparar joias.del\n"); return; }

    // garante índice
    FILE *t = fopen(idx_path, "rb");
    if (!t) {
        if (!rebuild_index(dados_path, del_path, idx_path)) return;
    } else fclose(t);

    int qtd = 0;
    Indice *indice = carregarIndice(idx_path, &qtd);
    if (!indice || qtd == 0) { printf("Falha ao carregar indice.\n"); if (indice) free(indice); return; }

    unsigned int chave;
    printf("Registro ID a remover: ");
    if (scanf("%u", &chave) != 1) { printf("Entrada invalida.\n"); free(indice); return; }

    long off0 = buscaBinariaIndice(indice, qtd, (uint64_t)chave);
    long recno = -1;
    long pos = scan_por_chave(dados_path, del_path, off0, chave, &recno);
    if (pos < 0) { printf("Registro nao encontrado.\n"); free(indice); return; }

    FILE *fdel = fopen(del_path, "rb+");
    if (!fdel) { printf("Erro ao abrir %s\n", del_path); free(indice); return; }
    if (!set_del_flag(fdel, recno, 1)) { printf("Falha ao marcar flag.\n"); fclose(fdel); free(indice); return; }
    fclose(fdel);

    rebuild_index(dados_path, del_path, idx_path);
    printf("Registro %u marcado como removido.\n", chave);

    free(indice);
}

// ------------------------ menu ------------------------

static void limpar_buffer() {
    int c; while ((c = getchar()) != '\n' && c != EOF) {}
}

int main(void) {
    const char *dados_path = "dados/joias.dat";
    const char *del_path   = "dados/joias.del";
    const char *idx_path   = "dados/joias.idx";

    if (!ensure_del_file(dados_path, del_path)) {
        printf("Preparando arquivos auxiliares...\n");
        if (!ensure_del_file(dados_path, del_path)) {
            printf("Falha ao preparar joias.del\n");
            return 1;
        }
    }

    int opc;
    do {
        printf("\n===== MENU JOIAS (chave: registro_id) =====\n");
        printf("1. Inserir novo registro\n");
        printf("2. Remover registro\n");
        printf("3. Mostrar primeiros registros\n");
        printf("4. Consultar por chave (via indice)\n");
        printf("5. Reconstruir indice\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        if (scanf("%d", &opc) != 1) { opc = 0; }
        limpar_buffer();

        switch (opc) {
            case 1: inserirJoiaOrdenada(dados_path, del_path, idx_path); break;
            case 2: removerJoia(dados_path, del_path, idx_path); break;
            case 3: {
                int n = 5;
                printf("Quantos registros exibir: ");
                if (scanf("%d", &n) != 1 || n <= 0) n = 5;
                limpar_buffer();
                mostrarPrimeirosRegistros(dados_path, del_path, n);
            } break;
            case 4: consultarPorChave(dados_path, del_path, idx_path); break;
            case 5: if (rebuild_index(dados_path, del_path, idx_path)) printf("Indice reconstruido.\n"); else printf("Falha ao reconstruir indice.\n"); break;
            case 0: break;
            default: printf("Opcao invalida.\n");
        }
    } while (opc != 0);

    return 0;
}
