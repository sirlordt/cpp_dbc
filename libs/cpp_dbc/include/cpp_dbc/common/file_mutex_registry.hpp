// file_mutex_registry.hpp
// Sistema de lock global asociado a archivos en disco

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <filesystem>

namespace cpp_dbc
{

    /**
     * @brief Registro global de mutexes asociados a archivos de base de datos
     *
     * Proporciona un mutex compartido por todas las conexiones al mismo archivo.
     * Thread-safe y usa weak_ptr para auto-limpieza.
     *
     * Beneficios:
     * - Elimina false positives de ThreadSanitizer
     * - Serializa operaciones al mismo archivo
     * - Compatible con locks internos de SQLite
     */
    class FileMutexRegistry
    {
    public:
        using MutexPtr = std::shared_ptr<std::recursive_mutex>;

        /**
         * @brief Obtiene la instancia singleton del registro
         */
        static FileMutexRegistry &getInstance()
        {
            static FileMutexRegistry instance;
            return instance;
        }

        /**
         * @brief Normaliza un path de archivo
         *
         * Resuelve:
         * - Symlinks
         * - Rutas relativas (./file.db)
         * - Slashes duplicados (/tmp//file.db)
         * - . y .. (parent directories)
         *
         * @param path Path original
         * @return Path normalizado y absoluto
         */
        static std::string normalizePath(const std::string &path)
        {
            try
            {
                namespace fs = std::filesystem;

                // Convertir a path absoluto y resolver symlinks
                fs::path fsPath(path);

                // Si el archivo existe, usar canonical (resuelve symlinks)
                if (fs::exists(fsPath))
                {
                    return fs::canonical(fsPath).string();
                }
                else
                {
                    // Si no existe, usar absolute + lexically_normal
                    // (útil al crear nuevas DBs)
                    return fs::absolute(fsPath).lexically_normal().string();
                }
            }
            catch (const std::filesystem::filesystem_error &)
            {
                // En caso de error, retornar el path original
                // (mejor algo de sincronización que nada)
                return path;
            }
        }

        /**
         * @brief Obtiene o crea un mutex para un archivo de base de datos
         *
         * @param dbPath Ruta al archivo de base de datos (será normalizada)
         * @return shared_ptr al mutex compartido para ese archivo
         *
         * Thread-safe: Múltiples threads pueden llamar concurrentemente
         */
        MutexPtr getMutexForFile(const std::string &dbPath)
        {
            // Normalizar el path (resolver symlinks, ./, //, etc.)
            std::string normalizedPath = normalizePath(dbPath);

            std::lock_guard<std::mutex> lock(m_registryMutex);

            // Intentar obtener el mutex existente
            auto it = m_mutexMap.find(normalizedPath);

            if (it != m_mutexMap.end())
            {
                // Existe un weak_ptr, intentar obtener shared_ptr
                if (auto sharedMutex = it->second.lock())
                {
                    return sharedMutex; // Mutex existente y válido
                }
                else
                {
                    // El mutex fue destruido, eliminar entrada obsoleta
                    m_mutexMap.erase(it);
                }
            }

            // Crear nuevo mutex para este archivo
            auto newMutex = std::make_shared<std::recursive_mutex>();
            m_mutexMap[normalizedPath] = newMutex; // Guardar como weak_ptr

            return newMutex;
        }

        /**
         * @brief Limpia entradas obsoletas del registro
         *
         * Elimina weak_ptrs que ya no apuntan a mutexes válidos.
         * Útil para evitar crecimiento ilimitado del mapa.
         */
        void cleanup()
        {
            std::lock_guard<std::mutex> lock(m_registryMutex);

            for (auto it = m_mutexMap.begin(); it != m_mutexMap.end();)
            {
                if (it->second.expired())
                {
                    it = m_mutexMap.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        /**
         * @brief Obtiene estadísticas del registro
         */
        struct Stats
        {
            size_t totalEntries;
            size_t activeEntries;
            size_t expiredEntries;
        };

        Stats getStats() const
        {
            std::lock_guard<std::mutex> lock(m_registryMutex);

            Stats stats{0, 0, 0};
            stats.totalEntries = m_mutexMap.size();

            for (const auto &[path, weakMutex] : m_mutexMap)
            {
                if (weakMutex.expired())
                    stats.expiredEntries++;
                else
                    stats.activeEntries++;
            }

            return stats;
        }

    private:
        // Singleton: constructor privado
        FileMutexRegistry() = default;
        ~FileMutexRegistry() = default;

        // No copiable, no movible
        FileMutexRegistry(const FileMutexRegistry &) = delete;
        FileMutexRegistry &operator=(const FileMutexRegistry &) = delete;
        FileMutexRegistry(FileMutexRegistry &&) = delete;
        FileMutexRegistry &operator=(FileMutexRegistry &&) = delete;

    private:
        // Mutex que protege el acceso al mapa de mutexes
        mutable std::mutex m_registryMutex;

        // Mapa: path normalizado -> weak_ptr<mutex>
        // Usamos weak_ptr para auto-limpieza cuando nadie usa el mutex
        std::unordered_map<std::string, std::weak_ptr<std::recursive_mutex>> m_mutexMap;
    };

    /**
     * @brief RAII guard para lock automático basado en archivo
     *
     * Uso:
     *   FileMutexGuard guard("/path/to/database.db");
     *   // ... operaciones con la DB ...
     *   // Auto-unlock al salir del scope
     */
    class FileMutexGuard
    {
    public:
        explicit FileMutexGuard(const std::string &dbPath)
            : m_mutex(FileMutexRegistry::getInstance().getMutexForFile(dbPath)), m_lock(*m_mutex)
        {
        }

        // No copiable, no movible (RAII estricto)
        FileMutexGuard(const FileMutexGuard &) = delete;
        FileMutexGuard &operator=(const FileMutexGuard &) = delete;
        FileMutexGuard(FileMutexGuard &&) = delete;
        FileMutexGuard &operator=(FileMutexGuard &&) = delete;

    private:
        FileMutexRegistry::MutexPtr m_mutex;
        std::lock_guard<std::recursive_mutex> m_lock;
    };

} // namespace cpp_dbc
