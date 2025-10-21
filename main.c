#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define MAX_STR 30
#define INDICE_INTERVALO 200
#define MAX_REGISTROS 150000

typedef struct {
    unsigned int registro_id;      // chave única gerada sequencialmente
    uint64_t product_id;
    uint64_t category_id;
    char category_alias[MAX_STR];
    char brand[MAX_STR];
    double price_usd;
    char gender[MAX_STR];
} Joia;

typedef struct {
    uint64_t order_id;             // chave única natural
    uint64_t product_id;
    int sku_quantity;
    uint64_t user_id;
    char date_time[MAX_STR];
} Compra;

typedef struct {
    uint64_t chave;
    long posicao;
} Indice;

// ---------------------------------------------------------
// Funções auxiliares
// ---------------------------------------------------------
void preencherEspacos(char *dest, const char *orig) {
    strncpy(dest, orig ? orig : "", MAX_STR - 1);
    dest[MAX_STR - 1] = '\0';
    size_t len = strlen(dest);
    for (size_t i = len; i < MAX_STR - 1; i++)
        dest[i] = ' ';
    dest[MAX_STR - 1] = '\0';
}

int splitCamposCSV(char *linha, char campos[15][MAX_STR]) {
    int i = 0, j = 0, dentroAspas = 0;
    for (int k = 0; linha[k] != '\0' && linha[k] != '\n'; k++) {
        char c = linha[k];
        if (c == '"') { dentroAspas = !dentroAspas; continue; }
        if (c == ',' && !dentroAspas) { campos[i][j] = '\0'; i++; j = 0; continue; }
        if (j < MAX_STR - 1) campos[i][j++] = c;
    }
    campos[i][j] = '\0';
    i++;
    for (; i < 15; i++) campos[i][0] = '\0';
    return i;
}

int cmpJoias(const void *a, const void *b) {
    const Joia *ja = (const Joia*)a;
    const Joia *jb = (const Joia*)b;
    if (ja->registro_id < jb->registro_id) return -1;
    if (ja->registro_id > jb->registro_id) return 1;
    return 0;
}

int cmpCompras(const void *a, const void *b) {
    const Compra *ca = (const Compra*)a;
    const Compra *cb = (const Compra*)b;
    if (ca->order_id < cb->order_id) return -1;
    if (ca->order_id > cb->order_id) return 1;
    return 0;
}

// ---------------------------------------------------------
// Geração dos arquivos
// ---------------------------------------------------------
void gerarArquivos() {
    FILE *csv = fopen("jewelry.csv", "r");
    if (!csv) {
        printf("Erro ao abrir jewelry.csv\n");
        return;
    }

#if defined(_WIN32)
    system("mkdir dados >nul 2>nul");
#else
    system("mkdir -p dados");
#endif

    Joia *vetJoias = (Joia*) malloc(MAX_REGISTROS * sizeof(Joia));
    Compra *vetCompras = (Compra*) malloc(MAX_REGISTROS * sizeof(Compra));

    if (!vetJoias || !vetCompras) {
        printf("Erro de memória.\n");
        fclose(csv);
        return;
    }

    char linha[1024];
    fgets(linha, sizeof(linha), csv);

    char campos[15][MAX_STR];
    int count = 0;

    while (fgets(linha, sizeof(linha), csv) && count < MAX_REGISTROS) {
        int qtd = splitCamposCSV(linha, campos);
        if (qtd < 10) continue;

        const char *date_time = campos[0];
        uint64_t order_id     = strtoull(campos[1], NULL, 10);
        uint64_t product_id   = strtoull(campos[2], NULL, 10);
        int sku_quantity      = atoi(campos[3]);
        uint64_t category_id  = strtoull(campos[4], NULL, 10);
        const char *alias     = campos[5];
        const char *brand     = campos[6];
        double price_usd      = atof(campos[7]);
        uint64_t user_id      = strtoull(campos[8], NULL, 10);
        const char *gender    = campos[9];

        // ---- Preenche Joia ----
        Joia j;
        j.registro_id = count + 1; // chave única sequencial
        j.product_id = product_id;
        j.category_id = category_id;
        preencherEspacos(j.category_alias, alias);
        preencherEspacos(j.brand, brand);
        j.price_usd = price_usd;
        preencherEspacos(j.gender, gender);
        vetJoias[count] = j;

        // ---- Preenche Compra ----
        Compra c;
        c.order_id = order_id;
        c.product_id = product_id;
        c.sku_quantity = sku_quantity;
        c.user_id = user_id;
        preencherEspacos(c.date_time, date_time);
        vetCompras[count] = c;

        count++;
    }

    fclose(csv);

    // Ordenação por chave
    qsort(vetJoias, count, sizeof(Joia), cmpJoias);
    qsort(vetCompras, count, sizeof(Compra), cmpCompras);

    // Escrita dos arquivos ordenados
    FILE *fj = fopen("dados/joias.dat", "wb");
    FILE *fc = fopen("dados/compras.dat", "wb");
    if (!fj || !fc) {
        printf("Erro ao criar arquivos de saída.\n");
        free(vetJoias); free(vetCompras);
        return;
    }

    fwrite(vetJoias, sizeof(Joia), count, fj);
    fwrite(vetCompras, sizeof(Compra), count, fc);
    fclose(fj); fclose(fc);

    // Geração dos índices
    FILE *idxJ = fopen("dados/joias.idx", "wb");
    FILE *idxC = fopen("dados/compras.idx", "wb");
    if (!idxJ || !idxC) {
        printf("Erro ao criar índices.\n");
        free(vetJoias); free(vetCompras);
        return;
    }

    Indice idx;
    for (int i = 0; i < count; i++) {
        if (i % INDICE_INTERVALO == 0) {
            idx.chave = vetJoias[i].registro_id;
            idx.posicao = (long)(i * sizeof(Joia));
            fwrite(&idx, sizeof(Indice), 1, idxJ);
        }
    }

    for (int i = 0; i < count; i++) {
        if (i % INDICE_INTERVALO == 0) {
            idx.chave = vetCompras[i].order_id;
            idx.posicao = (long)(i * sizeof(Compra));
            fwrite(&idx, sizeof(Indice), 1, idxC);
        }
    }

    fclose(idxJ);
    fclose(idxC);

    free(vetJoias);
    free(vetCompras);

    printf("Arquivos gerados com sucesso!\n");
    printf("Joias: %d registros | Compras: %d registros\n", count, count);
}

// ---------------------------------------------------------
int main() {
    gerarArquivos();
    return 0;
}

