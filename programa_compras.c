// compras_program.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define MAX_STR 30
#define INDICE_INTERVALO 200

typedef struct {
    uint64_t order_id;
    uint64_t product_id;
    int sku_quantity;
    uint64_t user_id;
    char date_time[MAX_STR];
} Compra;

typedef struct {
    uint64_t chave;
    long posicao;
} Indice;

// ==================== Funções auxiliares ====================
void preencherEspacos(char *dest, const char *orig) {
    strncpy(dest, orig ? orig : "", MAX_STR - 1);
    dest[MAX_STR - 1] = '\0';
    size_t len = strlen(dest);
    for (size_t i = len; i < MAX_STR - 1; i++)
        dest[i] = ' ';
    dest[MAX_STR - 1] = '\0';
}

long contar_registros(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fclose(f);
    return tam / sizeof(Compra);
}

int ensure_del_file(const char *dat_path, const char *del_path) {
    long n = contar_registros(dat_path);
    FILE *fd = fopen(del_path, "rb");
    if (fd) {
        fseek(fd, 0, SEEK_END);
        long t = ftell(fd);
        fclose(fd);
        if (t == n) return 1;
    }
    fd = fopen(del_path, "wb");
    if (!fd) return 0;
    unsigned char zero = 0;
    for (long i = 0; i < n; i++) fwrite(&zero, 1, 1, fd);
    fclose(fd);
    return 1;
}

int get_del_flag(FILE *fdel, long recno, unsigned char *flag) {
    if (fseek(fdel, recno, SEEK_SET) != 0) return 0;
    return fread(flag, 1, 1, fdel) == 1;
}

int set_del_flag(FILE *fdel, long recno, unsigned char flag) {
    if (fseek(fdel, recno, SEEK_SET) != 0) return 0;
    return fwrite(&flag, 1, 1, fdel) == 1;
}

void print_compra(const Compra *c) {
    char dt[MAX_STR];
    memcpy(dt, c->date_time, MAX_STR);
    dt[MAX_STR - 1] = '\0';
    printf("Order ID: %" PRIu64 "\n", c->order_id);
    printf("Product ID: %" PRIu64 "\n", c->product_id);
    printf("User ID: %" PRIu64 "\n", c->user_id);
    printf("Quantidade: %d\n", c->sku_quantity);
    printf("Data/Hora: %s\n", dt);
    printf("------------------------------------\n");
}

// ==================== Índice ====================

int cmp_indice(const void *a, const void *b) {
    const Indice *ia = (const Indice*)a;
    const Indice *ib = (const Indice*)b;
    if (ia->chave < ib->chave) return -1;
    if (ia->chave > ib->chave) return 1;
    return 0;
}

int rebuild_index(const char *dat_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dat_path, del_path)) return 0;
    FILE *fd = fopen(dat_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    FILE *fi = fopen(idx_path, "wb");
    if (!fd || !fl || !fi) {
        if (fd) fclose(fd);
        if (fl) fclose(fl);
        if (fi) fclose(fi);
        return 0;
    }
    Compra c; unsigned char flag;
    long rec = 0, validos = 0;
    while (fread(&c, sizeof(Compra), 1, fd) == 1) {
        if (!get_del_flag(fl, rec, &flag)) break;
        if (flag == 0) {
            if (validos % INDICE_INTERVALO == 0) {
                Indice ix = { c.order_id, rec * sizeof(Compra) };
                fwrite(&ix, sizeof(Indice), 1, fi);
            }
            validos++;
        }
        rec++;
    }
    fclose(fd); fclose(fl); fclose(fi);
    return 1;
}

Indice* carregarIndice(const char *idx_path, int *qtd) {
    FILE *fi = fopen(idx_path, "rb");
    if (!fi) { *qtd = 0; return NULL; }
    fseek(fi, 0, SEEK_END);
    long tam = ftell(fi);
    rewind(fi);
    int n = (int)(tam / sizeof(Indice));
    if (n <= 0) { fclose(fi); *qtd = 0; return NULL; }
    Indice *v = (Indice*) malloc(n * sizeof(Indice));
    fread(v, sizeof(Indice), n, fi);
    fclose(fi);
    qsort(v, n, sizeof(Indice), cmp_indice);
    *qtd = n;
    return v;
}

long buscaBinariaIndice(Indice *v, int n, uint64_t chave) {
    int i = 0, f = n - 1;
    while (i <= f) {
        int m = (i + f) / 2;
        if (v[m].chave == chave) return v[m].posicao;
        if (v[m].chave < chave) i = m + 1; else f = m - 1;
    }
    if (i == 0) return 0;
    if (i >= n) return v[n - 1].posicao;
    return v[i - 1].posicao;
}

long scan_por_chave(const char *dat_path, const char *del_path, long offset_ini, uint64_t chave, long *out_recno) {
    FILE *fd = fopen(dat_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    if (!fd || !fl) { if (fd) fclose(fd); if (fl) fclose(fl); return -1; }
    if (fseek(fd, offset_ini, SEEK_SET) != 0) { fclose(fd); fclose(fl); return -1; }

    long rec = offset_ini / sizeof(Compra);
    Compra c; unsigned char flag;
    while (fread(&c, sizeof(Compra), 1, fd) == 1) {
        if (!get_del_flag(fl, rec, &flag)) break;
        if (flag == 0) {
            if (c.order_id == chave) {
                if (out_recno) *out_recno = rec;
                long pos = rec * sizeof(Compra);
                fclose(fd); fclose(fl);
                return pos;
            }
            if (c.order_id > chave) break;
        }
        rec++;
    }
    fclose(fd); fclose(fl);
    return -1;
}

// ==================== Operações ====================

void mostrarPrimeirosRegistros(const char *dat_path, const char *del_path, int n) {
    if (!ensure_del_file(dat_path, del_path)) return;
    FILE *fd = fopen(dat_path, "rb");
    FILE *fl = fopen(del_path, "rb");
    if (!fd || !fl) { if (fd) fclose(fd); if (fl) fclose(fl); printf("Erro ao abrir.\n"); return; }

    Compra c; unsigned char flag;
    int mostrados = 0; long rec = 0;
    while (mostrados < n && fread(&c, sizeof(Compra), 1, fd) == 1) {
        if (!get_del_flag(fl, rec, &flag)) break;
        if (flag == 0) { print_compra(&c); mostrados++; }
        rec++;
    }
    fclose(fd); fclose(fl);
    printf("Exibidos %d registros.\n", mostrados);
}

void consultarPorChave(const char *dat_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dat_path, del_path)) return;
    int qtd = 0; Indice *idx = carregarIndice(idx_path, &qtd);
    if (!idx || qtd == 0) { rebuild_index(dat_path, del_path, idx_path); idx = carregarIndice(idx_path, &qtd); }
    if (!idx) { printf("Erro ao carregar indice.\n"); return; }

    uint64_t chave;
    printf("Digite o order_id: ");
#if defined(_WIN32)
    if (scanf("%llu", &chave) != 1) { printf("Entrada invalida.\n"); free(idx); return; }
#else
    if (scanf("%" SCNu64, &chave) != 1) { printf("Entrada invalida.\n"); free(idx); return; }
#endif

    long off0 = buscaBinariaIndice(idx, qtd, chave);
    long recno = -1;
    long pos = scan_por_chave(dat_path, del_path, off0, chave, &recno);
    if (pos < 0) { printf("Registro nao encontrado.\n"); free(idx); return; }

    FILE *fd = fopen(dat_path, "rb");
    fseek(fd, pos, SEEK_SET);
    Compra c; fread(&c, sizeof(Compra), 1, fd);
    fclose(fd);
    printf("\nRegistro encontrado:\n------------------------------------\n");
    print_compra(&c);

    free(idx);
}

void inserirOrdenado(const char *dat_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dat_path, del_path)) return;

    Compra nova;
    printf("Digite o order_id (uint64): ");
#if defined(_WIN32)
    if (scanf("%llu", &nova.order_id) != 1) { printf("Entrada invalida.\n"); return; }
#else
    if (scanf("%" SCNu64, &nova.order_id) != 1) { printf("Entrada invalida.\n"); return; }
#endif

    printf("Product_id (uint64): ");
#if defined(_WIN32)
    if (scanf("%llu", &nova.product_id) != 1) { printf("Entrada invalida.\n"); return; }
#else
    if (scanf("%" SCNu64, &nova.product_id) != 1) { printf("Entrada invalida.\n"); return; }
#endif

    printf("User_id (uint64): ");
#if defined(_WIN32)
    if (scanf("%llu", &nova.user_id) != 1) { printf("Entrada invalida.\n"); return; }
#else
    if (scanf("%" SCNu64, &nova.user_id) != 1) { printf("Entrada invalida.\n"); return; }
#endif

    printf("Quantidade (int): ");
    if (scanf("%d", &nova.sku_quantity) != 1) nova.sku_quantity = 1;

    printf("Data/hora: ");
    int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
    char buf[128]; if (!fgets(buf, sizeof(buf), stdin)) strcpy(buf, "");
    buf[strcspn(buf, "\r\n")] = '\0';
    preencherEspacos(nova.date_time, buf);

    // inserção ordenada
    FILE *fin = fopen(dat_path, "rb");
    FILE *ftemp = fopen("dados/temp.dat", "wb");
    FILE *fdel = fopen(del_path, "rb");
    FILE *ftempDel = fopen("dados/temp.del", "wb");
    if (!fin || !ftemp || !fdel || !ftempDel) {
        printf("Erro nos arquivos temporarios.\n");
        if (fin) fclose(fin); if (ftemp) fclose(ftemp);
        if (fdel) fclose(fdel); if (ftempDel) fclose(ftempDel);
        return;
    }

    Compra atual; unsigned char flag;
    int inserido = 0; long rec = 0;
    while (fread(&atual, sizeof(Compra), 1, fin) == 1 && fread(&flag, 1, 1, fdel) == 1) {
        if (!inserido && nova.order_id < atual.order_id) {
            fwrite(&nova, sizeof(Compra), 1, ftemp);
            unsigned char zero = 0; fwrite(&zero, 1, 1, ftempDel);
            inserido = 1;
        }
        fwrite(&atual, sizeof(Compra), 1, ftemp);
        fwrite(&flag, 1, 1, ftempDel);
        rec++;
    }

    if (!inserido) {
        fwrite(&nova, sizeof(Compra), 1, ftemp);
        unsigned char zero = 0; fwrite(&zero, 1, 1, ftempDel);
    }

    fclose(fin); fclose(ftemp);
    fclose(fdel); fclose(ftempDel);
    remove(dat_path); rename("dados/temp.dat", dat_path);
    remove(del_path); rename("dados/temp.del", del_path);

    rebuild_index(dat_path, del_path, idx_path);
    printf("Insercao concluida (order_id = %" PRIu64 ")\n", nova.order_id);
}

void removerCompra(const char *dat_path, const char *del_path, const char *idx_path) {
    if (!ensure_del_file(dat_path, del_path)) return;
    int qtd = 0; Indice *idx = carregarIndice(idx_path, &qtd);
    if (!idx || qtd == 0) { rebuild_index(dat_path, del_path, idx_path); idx = carregarIndice(idx_path, &qtd); }
    if (!idx) { printf("Erro ao carregar indice.\n"); return; }

    uint64_t chave;
    printf("Digite o order_id a remover: ");
#if defined(_WIN32)
    if (scanf("%llu", &chave) != 1) { printf("Entrada invalida.\n"); free(idx); return; }
#else
    if (scanf("%" SCNu64, &chave) != 1) { printf("Entrada invalida.\n"); free(idx); return; }
#endif

    long off0 = buscaBinariaIndice(idx, qtd, chave);
    long recno = -1;
    long pos = scan_por_chave(dat_path, del_path, off0, chave, &recno);
    if (pos < 0) { printf("Registro nao encontrado.\n"); free(idx); return; }

    FILE *fdel = fopen(del_path, "rb+");
    if (!fdel) { printf("Erro ao abrir .del\n"); free(idx); return; }
    set_del_flag(fdel, recno, 1);
    fclose(fdel);

    rebuild_index(dat_path, del_path, idx_path);
    printf("Registro %" PRIu64 " marcado como removido.\n", chave);
    free(idx);
}

// ==================== Menu ====================

void limpar_buffer() { int c; while ((c = getchar()) != '\n' && c != EOF) {} }

int main(void) {
    const char *dat_path = "dados/compras.dat";
    const char *del_path = "dados/compras.del";
    const char *idx_path = "dados/compras.idx";

    ensure_del_file(dat_path, del_path);

    int opc;
    do {
        printf("\n===== MENU COMPRAS (chave: order_id) =====\n");
        printf("1. Inserir nova compra\n");
        printf("2. Remover compra\n");
        printf("3. Mostrar primeiros registros\n");
        printf("4. Consultar por chave (via indice)\n");
        printf("5. Reconstruir indice\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        if (scanf("%d", &opc) != 1) { opc = 0; }
        limpar_buffer();

        switch (opc) {
            case 1: inserirOrdenado(dat_path, del_path, idx_path); break;
            case 2: removerCompra(dat_path, del_path, idx_path); break;
            case 3: {
                int n = 5;
                printf("Quantos registros exibir? (padrao 5): ");
                if (scanf("%d", &n) != 1 || n <= 0) n = 5;
                limpar_buffer();
                mostrarPrimeirosRegistros(dat_path, del_path, n);
            } break;
            case 4: consultarPorChave(dat_path, del_path, idx_path); break;
            case 5: if (rebuild_index(dat_path, del_path, idx_path)) printf("Indice reconstruido.\n"); else printf("Falha.\n"); break;
            case 0: break;
            default: printf("Opcao invalida.\n");
        }
    } while (opc != 0);
    return 0;
}
