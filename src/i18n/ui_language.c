#include "ui_language.h"

#include <string.h>

typedef struct {
    const char *key;
    const char *it;
    const char *en;
    const char *es;
    const char *pt;
    const char *fr;
} HelixTranslation;

static const HelixTranslation HELIX_TRANSLATIONS[] = {
    { "app.title", "Helix", "Helix", "Helix", "Helix", "Helix" },

    { "menu.file", "File", "File", "Archivo", "Arquivo", "Fichier" },
    { "menu.start_bot", "Start bot", "Start bot", "Iniciar bot", "Iniciar bot", "Démarrer le bot" },
    { "menu.stop_bot", "Stop bot", "Stop bot", "Detener bot", "Parar bot", "Arrêter le bot" },
    { "menu.quit", "Esci", "Quit", "Salir", "Sair", "Quitter" },

    { "menu.preferences", "Preferenze", "Preferences", "Preferencias", "Preferências", "Préférences" },
    { "menu.strategy_settings", "Impostazioni strategia", "Strategy settings", "Configuración de estrategia", "Configurações da estratégia", "Paramètres de stratégie" },
    { "menu.email_report", "Email report", "Email report", "Reporte email", "Relatório por email", "Rapport email" },
    { "menu.coinbase_api", "Coinbase API", "Coinbase API", "Coinbase API", "Coinbase API", "API Coinbase" },

    { "menu.view", "Visualizza", "View", "Ver", "Ver", "Affichage" },
    { "menu.trade_history", "Storico operazioni", "Trade history", "Historial de operaciones", "Histórico de operações", "Historique des opérations" },
    { "menu.engine_audit", "Audit decisioni", "Decision audit", "Auditoría de decisiones", "Auditoria de decisões", "Audit des décisions" },
    { "menu.real_slots", "Tabella slot reali", "Real slots table", "Tabla de slots reales", "Tabela de slots reais", "Table des slots réels" },
    { "menu.order_journal", "Tabella order journal", "Order journal table", "Tabla order journal", "Tabela order journal", "Table order journal" },
    { "menu.prelive_report", "Report pre-live", "Pre-live report", "Reporte pre-live", "Relatório pre-live", "Rapport pre-live" },
    { "menu.safety_status", "Stato protezioni", "Safety status", "Estado de protecciones", "Estado das proteções", "État des protections" },
    { "menu.dryrun_scenario", "Simula scenario dry-run", "Simulate dry-run scenario", "Simular escenario dry-run", "Simular cenário dry-run", "Simuler un scénario dry-run" },
    { "menu.export_prelive", "Esporta report pre-live", "Export pre-live report", "Exportar reporte pre-live", "Exportar relatório pre-live", "Exporter le rapport pre-live" },
    { "menu.export_snapshot", "Esporta snapshot stato", "Export status snapshot", "Exportar snapshot de estado", "Exportar snapshot de estado", "Exporter le snapshot d'état" },

    { "menu.help_root", "?", "?", "?", "?", "?" },
    { "menu.help", "Guida", "Help", "Ayuda", "Ajuda", "Aide" },
    { "menu.about", "Informazioni su...", "About...", "Acerca de...", "Sobre...", "À propos..." },

    { "section.strategy_settings", "Impostazioni strategia", "Strategy settings", "Configuración de estrategia", "Configurações da estratégia", "Paramètres de stratégie" },
    { "section.email_report", "Email report", "Email report", "Reporte email", "Relatório por email", "Rapport email" },
    { "section.coinbase_api", "Coinbase API", "Coinbase API", "Coinbase API", "Coinbase API", "API Coinbase" },
    { "section.trade_history", "Storico operazioni", "Trade history", "Historial de operaciones", "Histórico de operações", "Historique des opérations" },
    { "section.engine_audit", "Audit decisioni motore", "Engine decision audit", "Auditoría de decisiones del motor", "Auditoria de decisões do motor", "Audit des décisions moteur" },

    { "label.language", "Lingua interfaccia", "Interface language", "Idioma de la interfaz", "Idioma da interface", "Langue de l'interface" },
    { "label.line_unknown", "Linea/API: stato non ancora verificato", "Line/API: status not checked yet", "Línea/API: estado aún no verificado", "Linha/API: estado ainda não verificado", "Ligne/API : état pas encore vérifié" },

    { "button.save_settings", "Salva impostazioni", "Save settings", "Guardar configuración", "Salvar configurações", "Enregistrer les paramètres" },
    { "button.save_email", "Salva preferenze email", "Save email preferences", "Guardar preferencias de email", "Salvar preferências de email", "Enregistrer les préférences email" },
    { "button.test_email", "Test consegna email", "Test email delivery", "Probar envío de email", "Testar envio de email", "Tester l'envoi email" },
    { "button.activate_kill", "Attiva kill-switch", "Activate kill-switch", "Activar kill-switch", "Ativar kill-switch", "Activer le kill-switch" },
    { "button.reset_kill", "Reset kill-switch", "Reset kill-switch", "Reset kill-switch", "Reset kill-switch", "Réinitialiser le kill-switch" },
    { "button.arm_live", "Arma LIVE_TRADING", "Arm LIVE_TRADING", "Armar LIVE_TRADING", "Armar LIVE_TRADING", "Armer LIVE_TRADING" },
    { "button.disarm_live", "Disarma LIVE_TRADING", "Disarm LIVE_TRADING", "Desarmar LIVE_TRADING", "Desarmar LIVE_TRADING", "Désarmer LIVE_TRADING" },
    { "button.ack_order", "Acknowledge ultimo ordine reale", "Acknowledge last real order", "Confirmar última orden real", "Confirmar última ordem real", "Confirmer le dernier ordre réel" },
    { "button.seed_paper", "Seed paper slots demo", "Seed paper slots demo", "Crear slots paper demo", "Criar slots paper demo", "Créer des slots paper démo" },
    { "button.clear_paper", "Clear paper slots", "Clear paper slots", "Limpiar paper slots", "Limpar paper slots", "Effacer les slots paper" },
    { "button.run_best_profit", "Run paper BEST_PROFIT", "Run paper BEST_PROFIT", "Ejecutar paper BEST_PROFIT", "Executar paper BEST_PROFIT", "Exécuter paper BEST_PROFIT" },
    { "button.release_reserve", "Sblocca 1 slot riserva", "Release 1 reserve slot", "Liberar 1 slot de reserva", "Liberar 1 slot de reserva", "Libérer 1 slot de réserve" },
    { "button.lock_reserve", "Riblocca 1 slot riserva", "Lock 1 reserve slot", "Bloquear 1 slot de reserva", "Bloquear 1 slot de reserva", "Verrouiller 1 slot de réserve" },
    { "button.save_coinbase", "Salva credenziali Coinbase", "Save Coinbase credentials", "Guardar credenciales de Coinbase", "Salvar credenciais da Coinbase", "Enregistrer les identifiants Coinbase" },
    { "button.close", "Chiudi", "Close", "Cerrar", "Fechar", "Fermer" },
    { "button.calculate_scenario", "Calcola scenario", "Calculate scenario", "Calcular escenario", "Calcular cenário", "Calculer le scénario" },
    { "button.use_current_values", "Usa valori correnti", "Use current values", "Usar valores actuales", "Usar valores atuais", "Utiliser les valeurs actuelles" },

    { "dialog.info", "Operazione completata", "Operation completed", "Operación completada", "Operação concluída", "Opération terminée" },
    { "dialog.warning", "Attenzione", "Warning", "Atención", "Atenção", "Attention" },
    { "dialog.error", "Errore", "Error", "Error", "Erro", "Erreur" },

    { "empty.trades", "Nessuna operazione registrata", "No trades recorded", "No hay operaciones registradas", "Nenhuma operação registrada", "Aucune opération enregistrée" },
    { "empty.audit", "Nessuna decisione motore registrata", "No engine decisions recorded", "No hay decisiones del motor registradas", "Nenhuma decisão do motor registrada", "Aucune décision moteur enregistrée" },

    { "table.helix", "Tabella Helix", "Helix table", "Tabla Helix", "Tabela Helix", "Table Helix" },
    { "title.trade_history_full", "Storico operazioni - più recenti in alto", "Trade history - newest first", "Historial de operaciones - más recientes primero", "Histórico de operações - mais recentes primeiro", "Historique des opérations - plus récentes d'abord" },
    { "title.audit_full", "Audit decisioni motore - più recenti in alto", "Engine decision audit - newest first", "Auditoría de decisiones del motor - más recientes primero", "Auditoria de decisões do motor - mais recentes primeiro", "Audit des décisions moteur - plus récentes d'abord" },

    { "dashboard.eur_available", "EUR disponibili", "EUR available", "EUR disponibles", "EUR disponível", "EUR disponibles" },
    { "dashboard.btc_held", "BTC detenuti", "BTC held", "BTC mantenidos", "BTC mantido", "BTC détenus" },
    { "dashboard.slots_used", "Slot usati", "Used slots", "Slots usados", "Slots usados", "Slots utilisés" },
    { "dashboard.engine_mode", "Modalità motore", "Engine mode", "Modo motor", "Modo do motor", "Mode moteur" },
    { "dashboard.runtime_mode", "Modalità operativa", "Runtime mode", "Modo operativo", "Modo operacional", "Mode opérationnel" },
    { "dashboard.prelive_status", "Stato pre-live", "Pre-live status", "Estado pre-live", "Estado pre-live", "État pre-live" },
    { "dashboard.blocks", "blocchi", "blocks", "bloqueos", "bloqueios", "blocages" },
    { "dashboard.warnings", "warning", "warnings", "avisos", "avisos", "avertissements" },
    { "dashboard.api_health", "API health", "API health", "Salud API", "Saúde da API", "Santé API" },
    { "dashboard.last_trade", "Ultima operazione", "Last operation", "Última operación", "Última operação", "Dernière opération" },
    { "dashboard.strategy", "Strategia", "Strategy", "Estrategia", "Estratégia", "Stratégie" },
    { "dashboard.buy_drop", "buy drop", "buy drop", "caída compra", "queda compra", "baisse achat" },
    { "dashboard.sell", "sell", "sell", "venta", "venda", "vente" },
    { "dashboard.estimated_fee", "fee stimata", "estimated fee", "comisión estimada", "taxa estimada", "frais estimés" },
    { "dashboard.min_profit", "min profit", "min profit", "beneficio mín.", "lucro mín.", "profit min." },
    { "dashboard.min_liquidity", "liquidità min", "min liquidity", "liquidez mín.", "liquidez mín.", "liquidité min." },
    { "dashboard.reserve", "riserva", "reserve", "reserva", "reserva", "réserve" },
    { "dashboard.reserve_slots_unlocked", "slot riserva sbloccati", "reserve slots unlocked", "slots de reserva liberados", "slots de reserva liberados", "slots de réserve libérés" },
    { "dashboard.max_slots", "max slot", "max slots", "slots máx.", "slots máx.", "slots max." },
    { "dashboard.audit_days", "audit giorni", "audit days", "días audit", "dias audit", "jours audit" },
    { "dashboard.max_orders_day", "max ordini/giorno", "max orders/day", "órdenes máx./día", "ordens máx./dia", "ordres max/jour" },
    { "dashboard.cooldown", "cooldown", "cooldown", "cooldown", "cooldown", "cooldown" },
    { "dashboard.max_loss", "max loss", "max loss", "pérdida máx.", "perda máx.", "perte max." },
    { "dashboard.max_drawdown", "max drawdown", "max drawdown", "drawdown máx.", "drawdown máx.", "drawdown max." },
    { "dashboard.micro_live", "micro-live", "micro-live", "micro-live", "micro-live", "micro-live" },
    { "dashboard.max_micro_order", "max micro ordine", "max micro order", "orden micro máx.", "ordem micro máx.", "micro ordre max." },
    { "dashboard.stop_after_order", "stop dopo ordine", "stop after order", "parar tras orden", "parar após ordem", "arrêt après ordre" },
    { "dashboard.accumulation", "accumulo", "accumulation", "acumulación", "acumulação", "accumulation" },
    { "dashboard.kill_switch", "kill-switch", "kill-switch", "kill-switch", "kill-switch", "kill-switch" },
    { "dashboard.live_arm", "live arm", "live arm", "armado live", "arm live", "armement live" },
    { "status.active", "ATTIVO", "ACTIVE", "ACTIVO", "ATIVO", "ACTIF" },
    { "status.inactive", "disattivato", "disabled", "desactivado", "desativado", "désactivé" },
    { "status.started", "avviato", "running", "iniciado", "iniciado", "démarré" },
    { "status.stopped", "fermo", "stopped", "detenido", "parado", "arrêté" },
    { "status.protected", "protetta", "protected", "protegida", "protegida", "protégée" },
    { "status.partially_unlocked", "parzialmente sbloccata", "partially unlocked", "parcialmente liberada", "parcialmente liberada", "partiellement libérée" },
    { "setting.slot_eur", "Slot EUR", "Slot EUR", "Slot EUR", "Slot EUR", "Slot EUR" },
    { "setting.buy_drop", "Buy drop %", "Buy drop %", "Buy drop %", "Buy drop %", "Buy drop %" },
    { "setting.sell_profit_gross", "Sell profit lordo %", "Gross sell profit %", "Beneficio bruto venta %", "Lucro bruto venda %", "Profit brut vente %" },
    { "setting.estimated_fee", "Fee stimata %", "Estimated fee %", "Comisión estimada %", "Taxa estimada %", "Frais estimés %" },
    { "setting.min_profit_eur", "Profitto minimo EUR", "Minimum profit EUR", "Beneficio mínimo EUR", "Lucro mínimo EUR", "Profit minimum EUR" },
    { "setting.min_profit_percent", "Profitto minimo %", "Minimum profit %", "Beneficio mínimo %", "Lucro mínimo %", "Profit minimum %" },
    { "setting.min_liquidity", "Liquidità min %", "Minimum liquidity %", "Liquidez mínima %", "Liquidez mínima %", "Liquidité minimum %" },
    { "setting.protected_reserve", "Riserva protetta %", "Protected reserve %", "Reserva protegida %", "Reserva protegida %", "Réserve protégée %" },
    { "setting.reserve_slots_unlocked", "Slot riserva sbloccati", "Reserve slots unlocked", "Slots de reserva liberados", "Slots de reserva liberados", "Slots de réserve libérés" },
    { "setting.max_slots", "Max slot", "Max slots", "Slots máx.", "Slots máx.", "Slots max." },
    { "setting.audit_days", "Conserva audit giorni", "Keep audit days", "Conservar audit días", "Manter audit dias", "Conserver audit jours" },
    { "setting.volatility_window", "Volatilità finestra sec", "Volatility window sec", "Ventana volatilidad seg", "Janela volatilidade seg", "Fenêtre volatilité sec" },
    { "setting.volatility_max_move", "Volatilità max movimento %", "Volatility max move %", "Movimiento máx. volatilidad %", "Movimento máx. volatilidade %", "Mouvement max volatilité %" },
    { "setting.max_orders_day", "Max ordini al giorno", "Max orders per day", "Órdenes máx. por día", "Ordens máx. por dia", "Ordres max par jour" },
    { "setting.cooldown_sec", "Cooldown ordini sec", "Order cooldown sec", "Cooldown órdenes seg", "Cooldown ordens seg", "Cooldown ordres sec" },
    { "setting.max_daily_loss", "Max perdita giornaliera EUR", "Max daily loss EUR", "Pérdida diaria máx. EUR", "Perda diária máx. EUR", "Perte journalière max EUR" },
    { "setting.max_drawdown", "Max drawdown %", "Max drawdown %", "Drawdown máx. %", "Drawdown máx. %", "Drawdown max %" },
    { "setting.micro_live_enabled", "Micro-live attivo (0/1)", "Micro-live enabled (0/1)", "Micro-live activo (0/1)", "Micro-live ativo (0/1)", "Micro-live actif (0/1)" },
    { "setting.micro_live_max_order", "Micro-live max ordine EUR", "Micro-live max order EUR", "Orden micro-live máx. EUR", "Ordem micro-live máx. EUR", "Ordre micro-live max EUR" },
    { "setting.stop_after_real_order", "Stop dopo ordine reale (0/1)", "Stop after real order (0/1)", "Parar tras orden real (0/1)", "Parar após ordem real (0/1)", "Arrêt après ordre réel (0/1)" },
    { "setting.allow_micro_accumulation", "Consenti accumulo micro-live (0/1)", "Allow micro-live accumulation (0/1)", "Permitir acumulación micro-live (0/1)", "Permitir acumulação micro-live (0/1)", "Autoriser accumulation micro-live (0/1)" },
    { "setting.runtime_mode", "Modalità operativa", "Runtime mode", "Modo operativo", "Modo operacional", "Mode opérationnel" },
    { "section.daily_email_report", "Email report giornaliero", "Daily email report", "Reporte email diario", "Relatório diário por email", "Rapport email quotidien" },
    { "section.delivery_test", "Test consegna", "Delivery test", "Prueba de entrega", "Teste de entrega", "Test de livraison" },
    { "setting.email_enabled", "Email giornaliera attiva (0/1)", "Daily email enabled (0/1)", "Email diario activo (0/1)", "Email diário ativo (0/1)", "Email quotidien actif (0/1)" },
    { "setting.email_recipient", "Destinatario email", "Email recipient", "Destinatario email", "Destinatário email", "Destinataire email" },
    { "setting.email_hour", "Ora report email (0-23)", "Email report hour (0-23)", "Hora reporte email (0-23)", "Hora relatório email (0-23)", "Heure rapport email (0-23)" },
    { "setting.email_minute", "Minuto report email (0-59)", "Email report minute (0-59)", "Minuto reporte email (0-59)", "Minuto relatório email (0-59)", "Minute rapport email (0-59)" },
    { "setting.email_command", "Comando invio email", "Email send command", "Comando envío email", "Comando envio email", "Commande envoi email" },
    { "help.email_delivery", "Il test usa il comando configurato, ad esempio: sendmail -t. Se msmtp non è configurato, il test fallirà con il dettaglio dell'errore.", "The test uses the configured command, for example: sendmail -t. If msmtp is not configured, the test will fail with error details.", "La prueba usa el comando configurado, por ejemplo: sendmail -t. Si msmtp no está configurado, la prueba fallará con el detalle del error.", "O teste usa o comando configurado, por exemplo: sendmail -t. Se msmtp não estiver configurado, o teste falhará com os detalhes do erro.", "Le test utilise la commande configurée, par exemple : sendmail -t. Si msmtp n’est pas configuré, le test échouera avec le détail de l’erreur." },
    { "setting.api_key", "API Key", "API Key", "API Key", "API Key", "API Key" },
    { "setting.api_secret", "API Secret", "API Secret", "API Secret", "API Secret", "API Secret" },
    { "button.save_language", "Salva lingua", "Save language", "Guardar idioma", "Salvar idioma", "Enregistrer la langue" },
    { "dialog.ok", "OK", "OK", "OK", "OK", "OK" },
    { "message.settings_saved", "Impostazioni salvate", "Settings saved", "Configuración guardada", "Configurações salvas", "Paramètres enregistrés" },
    { "message.invalid_settings", "Errore: impostazioni non valide", "Error: invalid settings", "Error: configuración no válida", "Erro: configurações inválidas", "Erreur : paramètres invalides" },
    { "message.live_trading_saved_blocked", "LIVE_TRADING salvato ma bloccato dai safety checks", "LIVE_TRADING saved but blocked by safety checks", "LIVE_TRADING guardado pero bloqueado por safety checks", "LIVE_TRADING salvo mas bloqueado pelos safety checks", "LIVE_TRADING enregistré mais bloqué par les safety checks" },
    { "message.bot_started", "Bot avviato", "Bot started", "Bot iniciado", "Bot iniciado", "Bot démarré" },
    { "message.bot_stopped", "Bot fermato", "Bot stopped", "Bot detenido", "Bot parado", "Bot arrêté" },
    { "message.language_saved", "Lingua interfaccia salvata", "Interface language saved", "Idioma de interfaz guardado", "Idioma da interface salvo", "Langue de l’interface enregistrée" },
    { "report.prelive_unavailable", "Report pre-live non disponibile: stato applicazione non valido.", "Pre-live report unavailable: invalid application state.", "Reporte pre-live no disponible: estado de aplicación no válido.", "Relatório pre-live indisponível: estado da aplicação inválido.", "Rapport pre-live indisponible : état d’application invalide." },
    { "report.safety_unavailable", "Stato protezioni non disponibile: stato applicazione non valido.", "Safety status unavailable: invalid application state.", "Estado de protecciones no disponible: estado de aplicación no válido.", "Estado das proteções indisponível: estado da aplicação inválido.", "État des protections indisponible : état d’application invalide." },
    { "report.none", "nessuno", "none", "ninguno", "nenhum", "aucun" },
    { "report.no_blocks", "Nessun blocco registrato", "No blocks recorded", "No hay bloqueos registrados", "Nenhum bloqueio registrado", "Aucun blocage enregistré" },
    { "report.timestamp_unavailable", "timestamp non disponibile", "timestamp unavailable", "timestamp no disponible", "timestamp indisponível", "timestamp indisponible" },
    { "title.trades_table", "Tabella trades", "Trades table", "Tabla trades", "Tabela trades", "Table trades" },
    { "title.engine_audit_table", "Tabella engine audit", "Engine audit table", "Tabla engine audit", "Tabela engine audit", "Table engine audit" },
    { "title.help", "Guida Helix", "Helix Help", "Ayuda de Helix", "Ajuda Helix", "Aide Helix" },
    { "title.about", "Informazioni su Helix", "About Helix", "Acerca de Helix", "Sobre Helix", "À propos de Helix" },
    { "message.db_open_error", "Impossibile aprire data/helix.db", "Unable to open data/helix.db", "No se puede abrir data/helix.db", "Não foi possível abrir data/helix.db", "Impossible d’ouvrir data/helix.db" },
    { "message.rows_displayed", "Righe visualizzate", "Displayed rows", "Filas mostradas", "Linhas exibidas", "Lignes affichées" },
    { "message.read_error", "errore lettura", "read error", "error de lectura", "erro de leitura", "erreur de lecture" },
    { "dryrun.title", "Simula scenario dry-run", "Simulate dry-run scenario", "Simular escenario dry-run", "Simular cenário dry-run", "Simuler un scénario dry-run" },
    { "dryrun.description", "Inserisci valori ipotetici per vedere cosa farebbe Helix in dry-run. La simulazione non modifica wallet, DB, Coinbase o ordini.", "Enter hypothetical values to see what Helix would do in dry-run. The simulation does not modify wallet, DB, Coinbase or orders.", "Introduce valores hipotéticos para ver qué haría Helix en dry-run. La simulación no modifica wallet, DB, Coinbase ni órdenes.", "Insira valores hipotéticos para ver o que Helix faria em dry-run. A simulação não modifica wallet, DB, Coinbase ou ordens.", "Saisis des valeurs hypothétiques pour voir ce que ferait Helix en dry-run. La simulation ne modifie ni wallet, ni DB, ni Coinbase, ni ordres." },
    { "dryrun.eur", "EUR simulati", "Simulated EUR", "EUR simulados", "EUR simulados", "EUR simulés" },
    { "dryrun.btc", "BTC simulati", "Simulated BTC", "BTC simulados", "BTC simulados", "BTC simulés" },
    { "dryrun.price", "Prezzo BTC-EUR simulato", "Simulated BTC-EUR price", "Precio BTC-EUR simulado", "Preço BTC-EUR simulado", "Prix BTC-EUR simulé" },
    { "dryrun.used_slots", "Slot usati simulati", "Simulated used slots", "Slots usados simulados", "Slots usados simulados", "Slots utilisés simulés" },


    { "word.active", "ATTIVO", "ACTIVE", "ACTIVO", "ATIVO", "ACTIF" },
    { "word.disabled", "disattivato", "disabled", "desactivado", "desativado", "désactivé" },
    { "word.running", "avviato", "running", "en ejecución", "em execução", "démarré" },
    { "word.stopped", "fermo", "stopped", "detenido", "parado", "arrêté" },
    { "word.protected", "protetta", "protected", "protegida", "protegida", "protégée" },
    { "word.partially_released", "parzialmente sbloccata", "partially released", "parcialmente liberada", "parcialmente liberada", "partiellement libérée" },
    { "word.yes", "sì", "yes", "sí", "sim", "oui" },
    { "word.no", "no", "no", "no", "não", "non" },

    { "dashboard.eur_available_fmt", "EUR disponibili: %.2f", "Available EUR: %.2f", "EUR disponibles: %.2f", "EUR disponíveis: %.2f", "EUR disponibles : %.2f" },
    { "dashboard.btc_held_fmt", "BTC detenuti: %.8f", "BTC held: %.8f", "BTC mantenidos: %.8f", "BTC detidos: %.8f", "BTC détenus : %.8f" },
    { "dashboard.slots_used_fmt", "Slot usati: %d / %d", "Used slots: %d / %d", "Slots usados: %d / %d", "Slots usados: %d / %d", "Slots utilisés : %d / %d" },
    { "dashboard.engine_mode_fmt", "Modalità motore: %s", "Engine mode: %s", "Modo motor: %s", "Modo do motor: %s", "Mode moteur : %s" },
    { "dashboard.runtime_mode_fmt", "Modalità operativa: %s", "Runtime mode: %s", "Modo operativo: %s", "Modo operacional: %s", "Mode opérationnel : %s" },
    { "dashboard.prelive_fmt", "Stato pre-live: %s | blocchi %d | warning %d | %s", "Pre-live status: %s | blocks %d | warnings %d | %s", "Estado pre-live: %s | bloqueos %d | advertencias %d | %s", "Estado pre-live: %s | bloqueios %d | avisos %d | %s", "État pre-live : %s | blocages %d | avertissements %d | %s" },
    { "dashboard.api_health_fmt", "API health: %s | blocchi %d | warning %d | %s", "API health: %s | blocks %d | warnings %d | %s", "Salud API: %s | bloqueos %d | advertencias %d | %s", "Saúde da API: %s | bloqueios %d | avisos %d | %s", "Santé API : %s | blocages %d | avertissements %d | %s" },
    { "dashboard.last_operation_fmt", "Ultima operazione: %s", "Last operation: %s", "Última operación: %s", "Última operação: %s", "Dernière opération : %s" },
    { "dashboard.strategy_fmt", "Strategia: slot %.2f € | buy drop %.2f%% | sell %.2f%% | fee stimata %.2f%% | min profit %.2f € / %.2f%% | liquidità min %.2f%% | riserva %.2f%% | slot riserva sbloccati %d | max slot %d | audit %d giorni | max ordini/giorno %d | cooldown %d sec | max loss %.2f € | max drawdown %.2f%% | micro-live %s | max micro ordine %.2f € | stop dopo ordine %s | accumulo %s | kill-switch %s | live arm %s", "Strategy: slot %.2f € | buy drop %.2f%% | sell %.2f%% | estimated fee %.2f%% | min profit %.2f € / %.2f%% | min liquidity %.2f%% | reserve %.2f%% | released reserve slots %d | max slots %d | audit %d days | max orders/day %d | cooldown %d sec | max loss %.2f € | max drawdown %.2f%% | micro-live %s | max micro order %.2f € | stop after order %s | accumulation %s | kill-switch %s | live arm %s", "Estrategia: slot %.2f € | caída compra %.2f%% | venta %.2f%% | comisión estimada %.2f%% | beneficio mín %.2f € / %.2f%% | liquidez mín %.2f%% | reserva %.2f%% | slots reserva liberados %d | slots máx %d | auditoría %d días | órdenes máx/día %d | cooldown %d seg | pérdida máx %.2f € | drawdown máx %.2f%% | micro-live %s | orden micro máx %.2f € | parar tras orden %s | acumulación %s | kill-switch %s | live arm %s", "Estratégia: slot %.2f € | queda compra %.2f%% | venda %.2f%% | taxa estimada %.2f%% | lucro mín %.2f € / %.2f%% | liquidez mín %.2f%% | reserva %.2f%% | slots reserva liberados %d | slots máx %d | auditoria %d dias | ordens máx/dia %d | cooldown %d seg | perda máx %.2f € | drawdown máx %.2f%% | micro-live %s | ordem micro máx %.2f € | parar após ordem %s | acumulação %s | kill-switch %s | live arm %s", "Stratégie : slot %.2f € | baisse achat %.2f%% | vente %.2f%% | frais estimés %.2f%% | profit min %.2f € / %.2f%% | liquidité min %.2f%% | réserve %.2f%% | slots réserve libérés %d | slots max %d | audit %d jours | ordres max/jour %d | cooldown %d sec | perte max %.2f € | drawdown max %.2f%% | micro-live %s | ordre micro max %.2f € | arrêt après ordre %s | accumulation %s | kill-switch %s | live arm %s" },

    { "report.general_status", "STATO GENERALE", "GENERAL STATUS", "ESTADO GENERAL", "ESTADO GERAL", "ÉTAT GÉNÉRAL" },
    { "report.status", "Stato", "Status", "Estado", "Estado", "État" },
    { "report.blocks", "Blocchi", "Blocks", "Bloqueos", "Bloqueios", "Blocages" },
    { "report.reason", "Motivo", "Reason", "Motivo", "Motivo", "Raison" },
    { "report.liquidity_reserve", "Riserva liquidità", "Liquidity reserve", "Reserva de liquidez", "Reserva de liquidez", "Réserve de liquidité" },
    { "report.released_slots", "slot sbloccati", "released slots", "slots liberados", "slots liberados", "slots libérés" },
    { "report.dryrun_journal", "DRY-RUN E JOURNAL", "DRY-RUN AND JOURNAL", "DRY-RUN Y JOURNAL", "DRY-RUN E JOURNAL", "DRY-RUN ET JOURNAL" },
    { "report.safety_blocks_7d", "SAFETY BLOCKS ULTIMI 7 GIORNI", "SAFETY BLOCKS LAST 7 DAYS", "BLOQUEOS DE SEGURIDAD ÚLTIMOS 7 DÍAS", "BLOQUEIOS DE SEGURANÇA ÚLTIMOS 7 DIAS", "BLOCAGES DE SÉCURITÉ 7 DERNIERS JOURS" },
    { "report.recommendations", "RACCOMANDAZIONI", "RECOMMENDATIONS", "RECOMENDACIONES", "RECOMENDAÇÕES", "RECOMMANDATIONS" },
    { "report.unavailable_prelive", "Report pre-live non disponibile: stato applicazione non valido.", "Pre-live report unavailable: invalid application state.", "Reporte pre-live no disponible: estado de aplicación no válido.", "Relatório pre-live indisponível: estado da aplicação inválido.", "Rapport pre-live indisponible : état d'application invalide." },
    { "report.unavailable_safety", "Stato protezioni non disponibile: stato applicazione non valido.", "Safety status unavailable: invalid application state.", "Estado de seguridad no disponible: estado de aplicación no válido.", "Estado das proteções indisponível: estado da aplicação inválido.", "État des protections indisponible : état d'application invalide." },
    { "report.generated", "Generato", "Generated", "Generado", "Gerado", "Généré" },
    { "report.exported_prelive", "Report pre-live esportato in data/prelive_report.txt", "Pre-live report exported to data/prelive_report.txt", "Reporte pre-live exportado a data/prelive_report.txt", "Relatório pre-live exportado para data/prelive_report.txt", "Rapport pre-live exporté vers data/prelive_report.txt" },

    { "daily.title", "Helix - report giornaliero", "Helix - daily report", "Helix - reporte diario", "Helix - relatório diário", "Helix - rapport quotidien" },
    { "daily.local_date", "Data locale", "Local date", "Fecha local", "Data local", "Date locale" },
    { "daily.info_mode", "Modalità: report informativo. Nessun ordine viene eseguito da questa email.", "Mode: informational report. No order is executed from this email.", "Modo: reporte informativo. Ninguna orden se ejecuta desde este email.", "Modo: relatório informativo. Nenhuma ordem é executada por este email.", "Mode : rapport informatif. Aucun ordre n'est exécuté depuis cet email." },
    { "daily.current_state", "Stato attuale", "Current state", "Estado actual", "Estado atual", "État actuel" },
    { "daily.main_settings", "Impostazioni principali", "Main settings", "Configuración principal", "Configurações principais", "Paramètres principaux" },
    { "daily.open_real_slots", "Slot reali aperti", "Open real slots", "Slots reales abiertos", "Slots reais abertos", "Slots réels ouverts" },
    { "daily.no_open_real_slots", "Nessuno slot reale aperto.", "No open real slots.", "No hay slots reales abiertos.", "Nenhum slot real aberto.", "Aucun slot réel ouvert." },
    { "daily.no_recent_events", "Nessun evento recente.", "No recent events.", "No hay eventos recientes.", "Nenhum evento recente.", "Aucun événement récent." },

    { NULL, NULL, NULL, NULL, NULL, NULL }
};

const char *helix_language_code(HelixLanguage language) {
    switch (language) {
        case HELIX_LANG_EN: return "en";
        case HELIX_LANG_ES: return "es";
        case HELIX_LANG_PT: return "pt";
        case HELIX_LANG_FR: return "fr";
        case HELIX_LANG_IT:
        default: return "it";
    }
}

const char *helix_language_display_name(HelixLanguage language) {
    switch (language) {
        case HELIX_LANG_EN: return "English";
        case HELIX_LANG_ES: return "Español";
        case HELIX_LANG_PT: return "Português";
        case HELIX_LANG_FR: return "Français";
        case HELIX_LANG_IT:
        default: return "Italiano";
    }
}

HelixLanguage helix_language_from_code(const char *code) {
    if (code == NULL) {
        return HELIX_LANG_IT;
    }

    if (strcmp(code, "en") == 0) return HELIX_LANG_EN;
    if (strcmp(code, "es") == 0) return HELIX_LANG_ES;
    if (strcmp(code, "pt") == 0) return HELIX_LANG_PT;
    if (strcmp(code, "fr") == 0) return HELIX_LANG_FR;

    return HELIX_LANG_IT;
}

const char *helix_tr(HelixLanguage language, const char *key) {
    const HelixTranslation *item;

    if (key == NULL) {
        return "";
    }

    for (item = HELIX_TRANSLATIONS; item->key != NULL; item++) {
        if (strcmp(item->key, key) == 0) {
            switch (language) {
                case HELIX_LANG_EN: return item->en;
                case HELIX_LANG_ES: return item->es;
                case HELIX_LANG_PT: return item->pt;
                case HELIX_LANG_FR: return item->fr;
                case HELIX_LANG_IT:
                default: return item->it;
            }
        }
    }

    return key;
}
