 Documentación de la Biblioteca CPP_DBC

Este documento proporciona una guía completa de la biblioteca CPP_DBC, una biblioteca de Conectividad de Bases de Datos para C++ inspirada en JDBC.

## Tabla de Contenidos
- [Componentes Principales](#componentes-principales)
- [Implementación MySQL](#implementación-mysql)
- [Implementación PostgreSQL](#implementación-postgresql)
- [Pool de Conexiones](#pool-de-conexiones)
- [Gestor de Transacciones](#gestor-de-transacciones)
- [Sistema de Configuración](#sistema-de-configuración)
- [Compilación e Instalación](#compilación-e-instalación)

---

## Componentes Principales
*Componentes definidos en cpp_dbc.hpp*

### SQLException
Una clase de excepción personalizada para errores relacionados con SQL.

**Métodos:**
- `SQLException(const std::string& message)`: Constructor que toma un mensaje de error.

### Enum Types
Representa tipos de parámetros SQL.

**Valores:**
- `INTEGER`: Para valores enteros
- `FLOAT`: Para valores de punto flotante
- `DOUBLE`: Para valores de doble precisión
- `VARCHAR`: Para cadenas de caracteres de longitud variable
- `DATE`: Para valores de fecha
- `TIMESTAMP`: Para valores de marca de tiempo
- `BOOLEAN`: Para valores booleanos
- `BLOB`: Para objetos binarios grandes

### ResultSet
Una clase base abstracta que representa un conjunto de resultados de una consulta SQL.

**Métodos:**
- `next()`: Avanza a la siguiente fila, devuelve true si tiene éxito.
- `isBeforeFirst()`: Devuelve true si el cursor está antes de la primera fila.
- `isAfterLast()`: Devuelve true si el cursor está después de la última fila.
- `getRow()`: Devuelve el número de fila actual.
- `getInt(int/string)`: Recupera el valor de la columna designada como un entero.
- `getLong(int/string)`: Recupera el valor de la columna designada como un long.
- `getDouble(int/string)`: Recupera el valor de la columna designada como un double.
- `getString(int/string)`: Recupera el valor de la columna designada como una cadena.
- `getBoolean(int/string)`: Recupera el valor de la columna designada como un booleano.
- `isNull(int/string)`: Devuelve true si el valor de la columna designada es nulo.
- `getColumnNames()`: Devuelve un vector con todos los nombres de columnas.
- `getColumnCount()`: Devuelve el número de columnas en el conjunto de resultados.

### PreparedStatement
Una clase base abstracta que representa una declaración SQL precompilada.

**Métodos:**
- `setInt(int, int)`: Establece el parámetro designado al valor entero dado.
- `setLong(int, long)`: Establece el parámetro designado al valor long dado.
- `setDouble(int, double)`: Establece el parámetro designado al valor double dado.
- `setString(int, string)`: Establece el parámetro designado al valor de cadena dado.
- `setBoolean(int, bool)`: Establece el parámetro designado al valor booleano dado.
- `setNull(int, Types)`: Establece el parámetro designado a SQL NULL.
- `executeQuery()`: Ejecuta la consulta y devuelve un ResultSet.
- `executeUpdate()`: Ejecuta la actualización y devuelve el número de filas afectadas.
- `execute()`: Ejecuta la declaración SQL y devuelve true si el resultado es un ResultSet.

### Connection
Una clase base abstracta que representa una conexión a una base de datos.

**Métodos:**
- `close()`: Cierra la conexión.
- `isClosed()`: Devuelve true si la conexión está cerrada.
- `prepareStatement(string)`: Crea un PreparedStatement para la declaración SQL dada.
- `executeQuery(string)`: Ejecuta la consulta SQL dada y devuelve un ResultSet.
- `executeUpdate(string)`: Ejecuta la actualización SQL dada y devuelve el número de filas afectadas.
- `setAutoCommit(bool)`: Establece el modo de auto-commit.
- `getAutoCommit()`: Devuelve el modo de auto-commit.
- `commit()`: Confirma la transacción actual.
- `rollback()`: Revierte la transacción actual.

### Driver
Una clase base abstracta que representa un controlador de base de datos.

**Métodos:**
- `connect(string, string, string)`: Establece una conexión a la base de datos.
- `acceptsURL(string)`: Devuelve true si el controlador puede conectarse a la URL dada.

### DriverManager
Una clase gestora para registrar y recuperar instancias de controladores.

**Métodos:**
- `registerDriver(string, Driver)`: Registra un controlador con el nombre dado.
- `getConnection(string, string, string)`: Obtiene una conexión a la base de datos especificada por la URL.

---

## Implementación MySQL
*Componentes definidos en drivers/driver_mysql.hpp y drivers/driver_mysql.cpp*

### MySQLResultSet
Implementación de ResultSet para MySQL.

**Métodos:**
Los mismos que ResultSet, más:
- `MySQLResultSet(MYSQL_RES*)`: Constructor que toma un conjunto de resultados MySQL.

### MySQLPreparedStatement
Implementación de PreparedStatement para MySQL.

**Métodos:**
Los mismos que PreparedStatement, más:
- `MySQLPreparedStatement(MYSQL*, string)`: Constructor que toma una conexión MySQL y una declaración SQL.

### MySQLConnection
Implementación de Connection para MySQL.

**Métodos:**
Los mismos que Connection, más:
- `MySQLConnection(string, int, string, string, string)`: Constructor que toma host, puerto, base de datos, usuario y contraseña.

### MySQLDriver
Implementación de Driver para MySQL.

**Métodos:**
Los mismos que Driver, más:
- `MySQLDriver()`: Constructor que inicializa la biblioteca MySQL.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Implementación PostgreSQL
*Componentes definidos en drivers/driver_postgresql.hpp y drivers/driver_postgresql.cpp*

### PostgreSQLResultSet
Implementación de ResultSet para PostgreSQL.

**Métodos:**
Los mismos que ResultSet, más:
- `PostgreSQLResultSet(PGresult*)`: Constructor que toma un resultado PostgreSQL.

### PostgreSQLPreparedStatement
Implementación de PreparedStatement para PostgreSQL.

**Métodos:**
Los mismos que PreparedStatement, más:
- `PostgreSQLPreparedStatement(PGconn*, string)`: Constructor que toma una conexión PostgreSQL y una declaración SQL.

### PostgreSQLConnection
Implementación de Connection para PostgreSQL.

**Métodos:**
Los mismos que Connection, más:
- `PostgreSQLConnection(string, int, string, string, string)`: Constructor que toma host, puerto, base de datos, usuario y contraseña.
- `generateStatementName()`: Genera un nombre único para declaraciones preparadas.

### PostgreSQLDriver
Implementación de Driver para PostgreSQL.

**Métodos:**
Los mismos que Driver, más:
- `PostgreSQLDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Pool de Conexiones
*Componentes definidos en connection_pool.hpp y connection_pool.cpp*

### ConnectionPoolConfig
Estructura de configuración para pools de conexiones.

**Propiedades:**
- `url`: La URL de la base de datos
- `username`: El nombre de usuario de la base de datos
- `password`: La contraseña de la base de datos
- `initialSize`: Número inicial de conexiones (predeterminado 5)
- `maxSize`: Número máximo de conexiones (predeterminado 20)
- `minIdle`: Número mínimo de conexiones inactivas (predeterminado 3)
- `maxWaitMillis`: Tiempo máximo de espera para una conexión (predeterminado 30000 ms)
- `validationTimeoutMillis`: Tiempo de espera para validación de conexiones (predeterminado 5000 ms)
- `idleTimeoutMillis`: Tiempo máximo de inactividad para una conexión (predeterminado 300000 ms)
- `maxLifetimeMillis`: Vida máxima de una conexión (predeterminado 1800000 ms)
- `testOnBorrow`: Si se deben probar las conexiones antes de prestarlas (predeterminado true)
- `testOnReturn`: Si se deben probar las conexiones al devolverlas al pool (predeterminado false)
- `validationQuery`: Consulta utilizada para validar conexiones (predeterminado "SELECT 1")

### ConnectionPool
Gestiona un pool de conexiones de base de datos.

**Métodos:**
- `ConnectionPool(ConnectionPoolConfig)`: Constructor que toma una configuración de pool.
- `getConnection()`: Obtiene una conexión del pool.
- `getActiveConnectionCount()`: Devuelve el número de conexiones activas.
- `getIdleConnectionCount()`: Devuelve el número de conexiones inactivas.
- `getTotalConnectionCount()`: Devuelve el número total de conexiones.
- `close()`: Cierra el pool y todas las conexiones.
- `createConnection()`: Crea una nueva conexión física (interno).
- `createPooledConnection()`: Crea un nuevo envoltorio de conexión agrupada (interno).
- `validateConnection(Connection)`: Valida una conexión (interno).
- `returnConnection(PooledConnection)`: Devuelve una conexión al pool (interno).
- `maintenanceTask()`: Función de hilo de mantenimiento (interno).

### PooledConnection
Envuelve una conexión física para proporcionar funcionalidad de agrupación.

**Métodos:**
Los mismos que Connection, más:
- `PooledConnection(Connection, ConnectionPool*)`: Constructor que toma una conexión y un pool.
- `getCreationTime()`: Devuelve el tiempo de creación de la conexión.
- `getLastUsedTime()`: Devuelve el último tiempo de uso de la conexión.
- `setActive(bool)`: Establece si la conexión está activa.
- `isActive()`: Devuelve si la conexión está activa.
- `getUnderlyingConnection()`: Devuelve la conexión física subyacente.

---

## Gestor de Transacciones
*Componentes definidos en transaction_manager.hpp y transaction_manager.cpp*

### TransactionContext
Estructura para mantener el estado de la transacción.

**Propiedades:**
- `connection`: La conexión a la base de datos
- `creationTime`: El tiempo en que se creó la transacción
- `lastAccessTime`: El último tiempo en que se accedió a la transacción
- `transactionId`: El ID único de la transacción
- `active`: Si la transacción está activa

### TransactionManager
Gestiona transacciones a través de diferentes hilos.

**Métodos:**
- `TransactionManager(ConnectionPool&)`: Constructor que toma un pool de conexiones.
- `beginTransaction()`: Inicia una nueva transacción y devuelve su ID.
- `getTransactionConnection(string)`: Obtiene la conexión para una transacción.
- `commitTransaction(string)`: Confirma una transacción por su ID.
- `rollbackTransaction(string)`: Revierte una transacción por su ID.
- `isTransactionActive(string)`: Devuelve si una transacción está activa.
- `getActiveTransactionCount()`: Devuelve el número de transacciones activas.
- `setTransactionTimeout(long)`: Establece el tiempo de espera de la transacción.
- `close()`: Cierra el gestor de transacciones.
- `cleanupTask()`: Función de hilo de limpieza (interno).
- `generateUUID()`: Genera un ID único (interno).

---

## Sistema de Configuración
*Componentes definidos en config/database_config.hpp y config/yaml_config_loader.hpp*

### DatabaseConfig
Representa una configuración de base de datos.

**Métodos:**
- `setName(string)`: Establece el nombre de la configuración de la base de datos.
- `getName()`: Obtiene el nombre de la configuración de la base de datos.
- `setType(string)`: Establece el tipo de base de datos (mysql, postgresql, etc.).
- `getType()`: Obtiene el tipo de base de datos.
- `setHost(string)`: Establece el host de la base de datos.
- `getHost()`: Obtiene el host de la base de datos.
- `setPort(int)`: Establece el puerto de la base de datos.
- `getPort()`: Obtiene el puerto de la base de datos.
- `setDatabase(string)`: Establece el nombre de la base de datos.
- `getDatabase()`: Obtiene el nombre de la base de datos.
- `setUsername(string)`: Establece el nombre de usuario de la base de datos.
- `getUsername()`: Obtiene el nombre de usuario de la base de datos.
- `setPassword(string)`: Establece la contraseña de la base de datos.
- `getPassword()`: Obtiene la contraseña de la base de datos.
- `setOption(string, string)`: Establece una opción específica de la base de datos.
- `getOption(string)`: Obtiene una opción específica de la base de datos.
- `getOptions()`: Obtiene todas las opciones específicas de la base de datos.
- `createConnectionString()`: Crea una cadena de conexión para esta configuración.

### ConnectionPoolConfig
Representa una configuración de pool de conexiones.

**Propiedades:**
- `initialSize`: Número inicial de conexiones
- `maxSize`: Número máximo de conexiones
- `connectionTimeout`: Tiempo de espera de conexión en milisegundos
- `idleTimeout`: Tiempo de espera de inactividad en milisegundos
- `validationInterval`: Intervalo de validación en milisegundos

**Métodos:**
- `setName(string)`: Establece el nombre de la configuración del pool de conexiones.
- `getName()`: Obtiene el nombre de la configuración del pool de conexiones.
- `setInitialSize(int)`: Establece el tamaño inicial del pool de conexiones.
- `getInitialSize()`: Obtiene el tamaño inicial del pool de conexiones.
- `setMaxSize(int)`: Establece el tamaño máximo del pool de conexiones.
- `getMaxSize()`: Obtiene el tamaño máximo del pool de conexiones.
- `setConnectionTimeout(int)`: Establece el tiempo de espera de conexión.
- `getConnectionTimeout()`: Obtiene el tiempo de espera de conexión.
- `setIdleTimeout(int)`: Establece el tiempo de espera de inactividad.
- `getIdleTimeout()`: Obtiene el tiempo de espera de inactividad.
- `setValidationInterval(int)`: Establece el intervalo de validación.
- `getValidationInterval()`: Obtiene el intervalo de validación.

### TestQueries
Representa una colección de consultas de prueba para diferentes tipos de bases de datos.

**Métodos:**
- `setConnectionTest(string)`: Establece la consulta de prueba de conexión.
- `getConnectionTest()`: Obtiene la consulta de prueba de conexión.
- `setQuery(string, string, string)`: Establece una consulta de prueba para un tipo específico de base de datos.
- `getQuery(string, string)`: Obtiene una consulta de prueba para un tipo específico de base de datos.
- `getQueries(string)`: Obtiene todas las consultas de prueba para un tipo específico de base de datos.

### DatabaseConfigManager
Gestiona configuraciones de bases de datos.

**Métodos:**
- `addDatabaseConfig(DatabaseConfig)`: Agrega una configuración de base de datos.
- `getDatabaseByName(string)`: Obtiene una configuración de base de datos por nombre.
- `getDatabases()`: Obtiene todas las configuraciones de bases de datos.
- `addConnectionPoolConfig(ConnectionPoolConfig)`: Agrega una configuración de pool de conexiones.
- `getConnectionPoolByName(string)`: Obtiene una configuración de pool de conexiones por nombre.
- `getConnectionPools()`: Obtiene todas las configuraciones de pools de conexiones.
- `setTestQueries(TestQueries)`: Establece las consultas de prueba.
- `getTestQueries()`: Obtiene las consultas de prueba.

### YamlConfigLoader
Carga configuraciones de bases de datos desde archivos YAML.

**Métodos:**
- `loadFromFile(string)`: Carga configuraciones de bases de datos desde un archivo YAML.
- `loadFromString(string)`: Carga configuraciones de bases de datos desde una cadena YAML.

---

## Compilación e Instalación

### Requisitos Previos
- Compilador de C++23
- Bibliotecas de desarrollo de MySQL (para soporte MySQL)
- Bibliotecas de desarrollo de PostgreSQL (para soporte PostgreSQL, opcional)
- Biblioteca yaml-cpp (para soporte de configuración YAML, opcional)
- CMake 3.15 o posterior
- Conan para gestión de dependencias

### Compilación con Scripts

La biblioteca proporciona scripts de compilación para simplificar el proceso:

```bash
# Compilar con soporte para MySQL solamente (por defecto)
./build.sh

# Compilar con soporte para MySQL y PostgreSQL
./build.sh --mysql --postgres

# Compilar con soporte para PostgreSQL solamente
./build.sh --mysql-off --postgres

# Habilitar soporte de configuración YAML
./build.sh --yaml

# Compilar ejemplos
./build.sh --examples

# Compilar pruebas
./build.sh --test

# Compilar en modo Release
./build.sh --release

# Habilitar YAML y compilar ejemplos
./build.sh --yaml --examples

# Compilar contenedor Docker
./build.dist.sh

# Compilar contenedor Docker con soporte PostgreSQL y YAML
./build.dist.sh --postgres --yaml
```

El script de compilación:
1. Verifica e instala las dependencias necesarias
2. Configura la biblioteca con CMake
3. Compila e instala la biblioteca en el directorio `build/libs/cpp_dbc`
4. Compila la aplicación principal

El script `build.dist.sh`:
1. Compila el proyecto con las opciones especificadas
2. Detecta automáticamente las dependencias de bibliotecas compartidas
3. Genera un Dockerfile con solo las dependencias necesarias
4. Compila y etiqueta la imagen Docker

### Script de Ayuda

El proyecto incluye un script de ayuda (`helper.sh`) que proporciona varias utilidades:

```bash
# Compilar el proyecto con soporte MySQL
./helper.sh --build

# Compilar con soporte PostgreSQL
./helper.sh --build --postgres

# Compilar con soporte YAML
./helper.sh --build --yaml

# Compilar y ejecutar pruebas
./helper.sh --test --run-test

# Verificar dependencias del ejecutable localmente
./helper.sh --ldd-bin

# Verificar dependencias del ejecutable dentro del contenedor
./helper.sh --ldd [nombre_contenedor]

# Ejecutar el ejecutable
./helper.sh --run-bin

# Se pueden combinar múltiples comandos
./helper.sh --build --yaml --examples --run-bin
```

### Ejecutar el Ejemplo de Configuración YAML

Para ejecutar el ejemplo de configuración YAML:

```bash
# Primero, compilar la biblioteca con soporte YAML y ejemplos
./build.sh --yaml --examples

# Luego ejecutar el ejemplo
./libs/cpp_dbc/examples/run_config_example.sh
```

Esto cargará el archivo de configuración YAML de ejemplo y mostrará las configuraciones de las bases de datos.

### Compilación Manual con CMake

También puedes compilar la biblioteca manualmente con CMake:

```bash
# Crear directorio de compilación para la biblioteca
mkdir -p libs/cpp_dbc/build
cd libs/cpp_dbc/build

# Configurar con CMake (MySQL habilitado, PostgreSQL deshabilitado, YAML deshabilitado)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar con soporte YAML
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Compilar e instalar
cmake --build . --target install

# Volver al directorio raíz
cd ../../..

# Crear directorio de compilación para la aplicación principal
mkdir -p build
cd build

# Configurar con CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configurar con soporte YAML
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=ON -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Compilar
cmake --build .
```

### Uso como Biblioteca

Para usar CPP_DBC en tu propio proyecto:

```cmake
# En tu CMakeLists.txt
find_package(cpp_dbc REQUIRED)

# Crear tu ejecutable o biblioteca
add_executable(tu_app main.cpp)

# Enlazar con cpp_dbc
target_link_libraries(tu_app PRIVATE cpp_dbc::cpp_dbc)

# Añadir definiciones de compilación para los controladores de base de datos
target_compile_definitions(tu_app PRIVATE
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
)
```

En tu código C++:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif

// Usar la biblioteca...

// Usando configuración YAML
#ifdef USE_CPP_YAML
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>

// Cargar configuración desde archivo YAML
cpp_dbc::config::DatabaseConfigManager configManager =
    cpp_dbc::config::YamlConfigLoader::loadFromFile("config.yml");

// Obtener una configuración específica de base de datos
const auto* dbConfig = configManager.getDatabaseByName("dev_mysql");
if (dbConfig) {
    // Usar la configuración para crear una conexión
    std::string connStr = dbConfig->createConnectionString();
    auto conn = cpp_dbc::DriverManager::getConnection(
        connStr, dbConfig->getUsername(), dbConfig->getPassword()
    );
    
    // Usar la conexión...
    conn->close();
}
#endif
