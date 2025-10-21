# Arquivos de Dados

# joias.dat

Chave primária: registro_id (gerado automaticamente, não repetido)
Campos: registro_id, product_id, category_id, category_alias, brand, price_usd, gender
Tipo de organização: Indexada-Sequencial
Ordenado por: registro_id
Campos com repetição: product_id, brand, gender
Arquivo de remoção lógica: joias.del (1 byte por registro: 0 = ativo, 1 = removido)
Arquivo de índice: joias.idx (índice parcial, 1 entrada a cada 200 registros)

# compras.dat

Chave primária: order_id (informado pelo usuário, único)
Campos: order_id, product_id, sku_quantity, user_id, date_time
Tipo de organização: Indexada-Sequencial
Ordenado por: order_id
Campos com repetição: product_id, user_id
Arquivo de remoção lógica: compras.del (1 byte por registro)
Arquivo de índice: compras.idx (índice parcial, 1 entrada a cada 200 registros)
