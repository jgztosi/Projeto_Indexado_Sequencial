## 🗂️ Descrição dos Arquivos de Dados

### **joias.dat**
- **Tipo de organização:** Indexada-Sequencial  
- **Chave primária:** `registro_id` (gerado automaticamente, único e não repetido)  
- **Campos:**  
  - `registro_id`  
  - `product_id`  
  - `category_id`  
  - `category_alias`  
  - `brand`  
  - `price_usd`  
  - `gender`  
- **Ordenação:** Arquivo ordenado pelo campo `registro_id`.  
- **Campos com repetição:** `product_id`, `brand`, `gender`  
- **Arquivo de remoção lógica:** `joias.del`  
  - 1 byte por registro (`0` = ativo, `1` = removido).  
  - Mantém o histórico de exclusões sem necessidade de reorganizar o arquivo principal.  
- **Observações:**  
  - Cada registro possui tamanho fixo (uso de `MAX_STR` = 30 para campos de texto).  
  - Inserções geram automaticamente o próximo `registro_id` e mantêm o arquivo ordenado.  
  - O arquivo é acessado diretamente em disco via `fseek()` e `fread()`.

---

### **compras.dat**
- **Tipo de organização:** Indexada-Sequencial  
- **Chave primária:** `order_id` (informado pelo usuário, único e não repetido)  
- **Campos:**  
  - `order_id`  
  - `product_id`  
  - `sku_quantity`  
  - `user_id`  
  - `date_time`  
- **Ordenação:** Arquivo ordenado pelo campo `order_id`.  
- **Campos com repetição:** `product_id`, `user_id`  
- **Arquivo de remoção lógica:** `compras.del`  
  - 1 byte por registro (`0` = ativo, `1` = removido).  
- **Observações:**  
  - Registros de tamanho fixo (campos `char` com 30 bytes).  
  - Inserções são feitas de forma ordenada diretamente no arquivo, sem carregar todos os dados em memória.  
  - A remoção é lógica (flag), e o índice é reconstruído após cada modificação.

---

## 📑 Descrição dos Arquivos de Índice

### **joias.idx**
- **Finalidade:** Acelerar buscas por `registro_id` no arquivo `joias.dat`.  
- **Chave indexada:** `registro_id`.  
- **Tipo:** Índice **parcial** (não contém todas as chaves).  
- **Critério de amostragem:**  
  - Uma entrada a cada 200 registros válidos (`INDICE_INTERVALO = 200`).  
- **Campos armazenados:**  
  - `chave` → valor do campo `registro_id`;  
  - `posicao` → posição em bytes no arquivo `joias.dat` (`offset` para `fseek()`).  
- **Estrutura do índice:**  
  - Array de structs `Indice { uint64_t chave; long posicao; }`.  
- **Busca:**  
  - Realizada por **pesquisa binária** em memória no `.idx`.  
  - Em seguida, uma **varredura sequencial curta** no `joias.dat` a partir do offset encontrado.  
- **Reconstrução:**  
  - O índice é automaticamente recriado após inserções e remoções,  
    e pode ser reconstruído manualmente pelo menu de opções do programa.

---

### **compras.idx**
- **Finalidade:** Acelerar buscas por `order_id` no arquivo `compras.dat`.  
- **Chave indexada:** `order_id`.  
- **Tipo:** Índice **parcial**.  
- **Critério de amostragem:**  
  - Uma entrada a cada 200 registros válidos.  
- **Campos armazenados:**  
  - `chave` → valor do campo `order_id`;  
  - `posicao` → deslocamento (offset) dentro do arquivo `compras.dat`.  
- **Estrutura do índice:**  
  - Struct `Indice { uint64_t chave; long posicao; }`.  
- **Busca:**  
  - Binária no índice (`.idx`),  
  - seguida de varredura sequencial no arquivo principal (`.dat`).  
- **Reconstrução:**  
  - Automática após inserções ou remoções,  
  - e também disponível manualmente via menu.  
