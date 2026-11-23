-- ============================================================
-- Global Manager - Dump SQL (MariaDB (Red))
-- Modo          : Completo (Estructura + Datos + Ãndices + Triggers)
-- Origen        : mariadb://root@localhost:3306/qglobalmanager
-- Puesto        : #1 (slackware.slackware)
-- Fecha         : 2026-11-23 09:36:54
-- ============================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE DATABASE IF NOT EXISTS `qglobalmanager` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `qglobalmanager`;

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `app_settings`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `app_settings`;
CREATE TABLE `app_settings` (
  `key` varchar(190) NOT NULL,
  `value` text NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('company.business_type', 'Comercio', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('regional.currency_symbol', '$', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.elevation_allow_backup', '1', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.elevation_allow_db_admin', '1', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.elevation_allow_support', '1', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.elevation_duration_min', '20', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.elevation_notify_email', '0', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.idle_minutes', '15', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('security.warning_seconds', '30', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('setup.remind_create_users', '1', '2026-11-23 09:36:47');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('smtp.mode', 'BASIC', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('smtp.port', '465', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('smtp.provider', 'GMAIL', '2026-11-23 09:36:18');
INSERT INTO `app_settings` (`key`, `value`, `updated_at`) VALUES ('smtp.security', 'SSL', '2026-11-23 09:36:18');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `audit_log`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `audit_log`;
CREATE TABLE `audit_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user_id` int(11) NOT NULL DEFAULT -1,
  `user_email` varchar(190) NOT NULL,
  `user_full_name` varchar(500) NOT NULL DEFAULT '',
  `user_role` varchar(500) NOT NULL DEFAULT '',
  `module_id` varchar(190) NOT NULL,
  `target_table` varchar(500) NOT NULL DEFAULT '',
  `record_id` varchar(500) NOT NULL DEFAULT '',
  `action` varchar(190) NOT NULL,
  `old_values` text DEFAULT NULL,
  `new_values` text DEFAULT NULL,
  `auth_context` varchar(500) NOT NULL DEFAULT '',
  `workstation` varchar(500) NOT NULL DEFAULT '',
  `details` text DEFAULT NULL,
  `prev_hash` varchar(500) NOT NULL DEFAULT '',
  `row_hash` varchar(500) NOT NULL DEFAULT '',
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `audit_log` (`id`, `user_id`, `user_email`, `user_full_name`, `user_role`, `module_id`, `target_table`, `record_id`, `action`, `old_values`, `new_values`, `auth_context`, `workstation`, `details`, `prev_hash`, `row_hash`, `created_at`) VALUES (1, -1, 'develslack@gmail.com', 'Administrador', 'Administrador', 'setup', 'users', '', 'INITIAL_SETUP', '', '', 'SETUP_WIZARD', 'Puesto #1 (slackware.slackware)', 'Inicialización Puesto #1 Red (Admin creado)', 'GENESIS', 'd8c19bfa230e9dc54c47a2c464208e79de37c7a3deaf9929a939067d6055430c', '2026-11-23 12:36:47');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `document_types`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `document_types`;
CREATE TABLE `document_types` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `code` varchar(190) NOT NULL,
  `name` varchar(190) NOT NULL,
  `is_active` int(11) NOT NULL DEFAULT 1,
  `is_deprecated` int(11) NOT NULL DEFAULT 0,
  `deprecated_at` varchar(500) NOT NULL DEFAULT '',
  `deprecated_by` varchar(500) NOT NULL DEFAULT '',
  `replacement_id` int(11) DEFAULT NULL,
  `deprecation_note` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `code` (`code`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `document_types` (`id`, `code`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (1, 'DNI', 'Documento Nacional de Identidad', 1, 0, '', '', NULL, '');
INSERT INTO `document_types` (`id`, `code`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (2, 'CUIT', 'Clave Única de Identificación Tributaria', 1, 0, '', '', NULL, '');
INSERT INTO `document_types` (`id`, `code`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (3, 'CUIL', 'Clave Única de Identificación Laboral', 1, 0, '', '', NULL, '');
INSERT INTO `document_types` (`id`, `code`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (4, 'PAS', 'Pasaporte', 1, 0, '', '', NULL, '');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `elevation_tokens`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `elevation_tokens`;
CREATE TABLE `elevation_tokens` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `token_code` varchar(190) NOT NULL,
  `user_id` int(11) NOT NULL,
  `user_email` varchar(190) NOT NULL,
  `target_scope` varchar(190) NOT NULL,
  `reason` varchar(500) NOT NULL DEFAULT '',
  `issued_at` timestamp NOT NULL DEFAULT current_timestamp(),
  `expires_at` varchar(500) NOT NULL DEFAULT '',
  `revoked_at` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `token_code` (`token_code`),
  KEY `user_id` (`user_id`),
  CONSTRAINT `elevation_tokens_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `external_source_items`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `external_source_items`;
CREATE TABLE `external_source_items` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `source_id` int(11) NOT NULL,
  `item_key` varchar(190) NOT NULL,
  `item_label` varchar(500) NOT NULL DEFAULT '',
  `value_num` double DEFAULT NULL,
  `value_text` varchar(500) NOT NULL DEFAULT '',
  `updated_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`),
  UNIQUE KEY `source_id` (`source_id`,`item_key`),
  CONSTRAINT `external_source_items_ibfk_1` FOREIGN KEY (`source_id`) REFERENCES `external_sources` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `external_sources`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `external_sources`;
CREATE TABLE `external_sources` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(190) NOT NULL,
  `source_type` varchar(190) NOT NULL,
  `source_uri` varchar(500) NOT NULL DEFAULT '',
  `format` varchar(190) NOT NULL,
  `delimiter` varchar(500) NOT NULL DEFAULT '',
  `key_field` varchar(500) NOT NULL DEFAULT '',
  `label_field` varchar(500) NOT NULL DEFAULT '',
  `value_field` varchar(500) NOT NULL DEFAULT '',
  `target_module` varchar(500) NOT NULL DEFAULT '',
  `last_sync_at` varchar(500) NOT NULL DEFAULT '',
  `is_active` int(11) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `iva_conditions`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `iva_conditions`;
CREATE TABLE `iva_conditions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(190) NOT NULL,
  `is_active` int(11) NOT NULL DEFAULT 1,
  `is_deprecated` int(11) NOT NULL DEFAULT 0,
  `deprecated_at` varchar(500) NOT NULL DEFAULT '',
  `deprecated_by` varchar(500) NOT NULL DEFAULT '',
  `replacement_id` int(11) DEFAULT NULL,
  `deprecation_note` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `iva_conditions` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (1, 'Responsable Inscripto', 1, 0, '', '', NULL, '');
INSERT INTO `iva_conditions` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (2, 'Monotributista', 1, 0, '', '', NULL, '');
INSERT INTO `iva_conditions` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (3, 'Exento', 1, 0, '', '', NULL, '');
INSERT INTO `iva_conditions` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (4, 'Consumidor Final', 1, 0, '', '', NULL, '');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `payment_methods`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `payment_methods`;
CREATE TABLE `payment_methods` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(190) NOT NULL,
  `is_active` int(11) NOT NULL DEFAULT 1,
  `is_deprecated` int(11) NOT NULL DEFAULT 0,
  `deprecated_at` varchar(500) NOT NULL DEFAULT '',
  `deprecated_by` varchar(500) NOT NULL DEFAULT '',
  `replacement_id` int(11) DEFAULT NULL,
  `deprecation_note` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `payment_methods` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (1, 'Efectivo', 1, 0, '', '', NULL, '');
INSERT INTO `payment_methods` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (2, 'Transferencia Bancaria', 1, 0, '', '', NULL, '');
INSERT INTO `payment_methods` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (3, 'Tarjeta de Débito', 1, 0, '', '', NULL, '');
INSERT INTO `payment_methods` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (4, 'Tarjeta de Crédito', 1, 0, '', '', NULL, '');
INSERT INTO `payment_methods` (`id`, `name`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (5, 'Billetera Virtual / QR', 1, 0, '', '', NULL, '');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `permissions`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `permissions`;
CREATE TABLE `permissions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `module_id` varchar(190) NOT NULL,
  `action` varchar(190) NOT NULL,
  `description` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `module_id` (`module_id`,`action`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (1, 'panel_control', 'access', 'Acceder al Panel de Control');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (2, 'panel_control', 'company', 'Panel de Control: Datos de la Compañía');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (3, 'panel_control', 'smtp', 'Panel de Control: Correo / SMTP');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (4, 'panel_control', 'printers', 'Panel de Control: Impresoras del Sistema');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (5, 'panel_control', 'security', 'Panel de Control: Seguridad y Licencia (Sensible)');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (6, 'panel_control', 'support_tables', 'Panel de Control: Tablas Soporte (Sensible)');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (7, 'panel_control', 'backup_audit', 'Panel de Control: Respaldo y Auditoría (Sensible)');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (8, 'panel_control', 'db_admin', 'Panel de Control: Administrador SQL (Sensible)');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (9, 'panel_control', 'request_elevation', 'Solicitar token temporal de elevación');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (10, 'usuarios', 'access', 'Ver listado de usuarios');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (11, 'usuarios', 'create', 'Dar de alta nuevos usuarios');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (12, 'usuarios', 'edit', 'Editar datos de usuarios');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (13, 'usuarios', 'permissions', 'Modificar permisos de usuarios');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (14, 'usuarios', 'reset_password', 'Generar y reenviar nuevas contraseñas');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (15, 'usuarios', 'block', 'Bloquear o desbloquear usuarios');
INSERT INTO `permissions` (`id`, `module_id`, `action`, `description`) VALUES (16, 'roles', 'access', 'Administrar roles del sistema');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `role_permissions`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `role_permissions`;
CREATE TABLE `role_permissions` (
  `role_id` int(11) NOT NULL,
  `permission_id` int(11) NOT NULL,
  PRIMARY KEY (`role_id`,`permission_id`),
  KEY `permission_id` (`permission_id`),
  CONSTRAINT `role_permissions_ibfk_1` FOREIGN KEY (`role_id`) REFERENCES `roles` (`id`) ON DELETE CASCADE,
  CONSTRAINT `role_permissions_ibfk_2` FOREIGN KEY (`permission_id`) REFERENCES `permissions` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (2, 1);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (2, 2);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (2, 3);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (2, 4);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (2, 9);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (3, 1);
INSERT INTO `role_permissions` (`role_id`, `permission_id`) VALUES (3, 4);

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `roles`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `roles`;
CREATE TABLE `roles` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(190) NOT NULL,
  `description` varchar(500) NOT NULL DEFAULT '',
  `is_system` int(11) NOT NULL DEFAULT 0,
  `is_active` int(11) NOT NULL DEFAULT 1,
  `is_deprecated` int(11) NOT NULL DEFAULT 0,
  `deprecated_at` varchar(500) NOT NULL DEFAULT '',
  `deprecated_by` varchar(500) NOT NULL DEFAULT '',
  `replacement_id` int(11) DEFAULT NULL,
  `deprecation_note` varchar(500) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `roles` (`id`, `name`, `description`, `is_system`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (1, 'Administrador', 'Acceso total al sistema y Panel de Control', 1, 1, 0, '', '', NULL, '');
INSERT INTO `roles` (`id`, `name`, `description`, `is_system`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (2, 'Responsable / Supervisor', 'Gestión operativa y configuración de la compañía con elevación por token', 0, 1, 0, '', '', NULL, '');
INSERT INTO `roles` (`id`, `name`, `description`, `is_system`, `is_active`, `is_deprecated`, `deprecated_at`, `deprecated_by`, `replacement_id`, `deprecation_note`) VALUES (3, 'Operador', 'Operación diaria (vendedor, secretaria, profesional) según permisos asignados', 0, 1, 0, '', '', NULL, '');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `user_permissions`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `user_permissions`;
CREATE TABLE `user_permissions` (
  `user_id` int(11) NOT NULL,
  `permission_id` int(11) NOT NULL,
  `granted` int(11) NOT NULL DEFAULT 1,
  PRIMARY KEY (`user_id`,`permission_id`),
  KEY `permission_id` (`permission_id`),
  CONSTRAINT `user_permissions_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE,
  CONSTRAINT `user_permissions_ibfk_2` FOREIGN KEY (`permission_id`) REFERENCES `permissions` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `users`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `email` varchar(190) NOT NULL,
  `full_name` varchar(190) NOT NULL,
  `role_id` int(11) NOT NULL,
  `password_hash` varchar(190) NOT NULL,
  `is_active` int(11) NOT NULL DEFAULT 1,
  `must_change_password` int(11) NOT NULL DEFAULT 0,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`),
  UNIQUE KEY `email` (`email`),
  KEY `role_id` (`role_id`),
  CONSTRAINT `users_ibfk_1` FOREIGN KEY (`role_id`) REFERENCES `roles` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `users` (`id`, `email`, `full_name`, `role_id`, `password_hash`, `is_active`, `must_change_password`, `created_at`) VALUES (1, 'develslack@gmail.com', 'Administrador', 1, 'sha256$100000$92aa17462ba62cbc6f122d10ec432be1$f22d27c461358076062362a4aa27ab4765ddd5ddf60050e4238bc9ec0d654967', 1, 0, '2026-11-23 09:36:47');

-- ------------------------------------------------------------
-- Estructura e Ã­ndices de tabla `workstations`
-- ------------------------------------------------------------
DROP TABLE IF EXISTS `workstations`;
CREATE TABLE `workstations` (
  `terminal_number` int(11) NOT NULL,
  `hostname` varchar(190) NOT NULL,
  `description` varchar(500) NOT NULL DEFAULT '',
  `last_seen_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`terminal_number`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `workstations` (`terminal_number`, `hostname`, `description`, `last_seen_at`) VALUES (1, 'slackware.slackware', 'slackware.slackware', '2026-11-23 09:36:47');

SET FOREIGN_KEY_CHECKS = 1;
