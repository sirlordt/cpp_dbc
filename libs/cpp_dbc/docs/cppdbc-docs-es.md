# Documentación de la Biblioteca CPP_DBC

Este documento proporciona una guía completa de la biblioteca CPP_DBC, una biblioteca de Conectividad de Bases de Datos para C++ inspirada en JDBC, con soporte para bases de datos MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB y Redis. Los 7 controladores implementan el patrón dual-API nothrow-first: la lógica real en métodos `noexcept` que retornan `expected<T, DBException>`, la API con excepciones como wrappers delgados protegidos por `#ifdef __cpp_exceptions`, construcción mediante fábrica estática, e inicialización del controlador con double-checked locking. La biblioteca es totalmente compatible con builds `-fno-exceptions`. Todos los archivos de encabezado públicos incluyen documentación compatible con Doxygen (`/** @brief ... */`) con ejemplos de código en línea, etiquetas `@param`/`@return`/`@see`, y están listos para la generación de referencias de API en HTML/PDF.

## Tabla de Contenidos
- [Componentes Principales](#componentes-principales)
- [Soporte para BLOB](#soporte-para-blob)
- [Implementación MySQL](#implementación-mysql)
- [Implementación PostgreSQL](#implementación-postgresql)
- [Implementación SQLite](#implementación-sqlite)
- [Implementación Firebird](#implementación-firebird)
- [Implementación MongoDB](#implementación-mongodb)
- [Implementación ScyllaDB](#implementación-scylladb)
- [Implementación Redis](#implementación-redis)
- [Pool de Conexiones](#pool-de-conexiones)
- [Gestor de Transacciones](#gestor-de-transacciones)
- [Sistema de Configuración](#sistema-de-configuración)
- [Compilación e Instalación](#compilación-e-instalación)

## Documentación Relacionada
- **[Cómo Agregar Nuevos Drivers de Bases de Datos](how_add_new_db_drivers.md)**: Guía completa en 5 fases para agregar nuevos drivers (en inglés)
- **[Patrones de Manejo de Errores](error_handling_patterns.md)**: DBException, códigos de error y patrones de API sin excepciones (en inglés)
- **[Dependencias de Scripts Shell](shell_script_dependencies.md)**: Jerarquía de llamadas y dependencias de scripts (en inglés)

---

## Componentes Principales
*Componentes definidos en cpp_dbc.hpp*

### DBException
Clase de excepción `final` para errores relacionados con la base de datos. Almacenamiento híbrido fijo/dinámico, constructible `noexcept` (~120 bytes tamaño del objeto). Hereda de `std::exception`. Un buffer fijo de 79 bytes almacena el mark y hasta 64 caracteres de mensaje sin asignación de heap. Mensajes mayores a 64 caracteres se almacenan en un buffer de desborde (`shared_ptr<char[]>`) asignado en el heap; si la asignación falla, `what()` retorna el buffer fijo truncado (degradación elegante).

**Métodos:**
- `DBException(const std::string& mark, const std::string& message, std::shared_ptr<system_utils::CallStackCapture> callstack)`: Constructor (noexcept) que toma un código de error (12 caracteres), mensaje y pila de llamadas opcional.
- `what()`: Retorna el mensaje completo (`const char*`) del buffer de desborde si existe, del buffer fijo en caso contrario.
- `what_s()`: Devuelve el mensaje de error como `std::string_view` (misma fuente que `what()`).
- `getMark()`: Devuelve la marca única de error como `std::string_view` (siempre del buffer fijo).
- `getCallStack()`: Devuelve la pila de llamadas como `std::span<const system_utils::StackFrame>`.
- `printCallStack()`: Imprime la pila de llamadas en la salida de error estándar.

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

### Enum TransactionIsolationLevel
Representa niveles de aislamiento de transacciones siguiendo el estándar JDBC.

**Valores:**
- `TRANSACTION_NONE`: No se admiten transacciones
- `TRANSACTION_READ_UNCOMMITTED`: Permite lecturas sucias, lecturas no repetibles y lecturas fantasma
- `TRANSACTION_READ_COMMITTED`: Evita lecturas sucias, pero permite lecturas no repetibles y lecturas fantasma
- `TRANSACTION_REPEATABLE_READ`: Evita lecturas sucias y lecturas no repetibles, pero permite lecturas fantasma
- `TRANSACTION_SERIALIZABLE`: Evita lecturas sucias, lecturas no repetibles y lecturas fantasma

### InputStream
Una clase base abstracta que representa un flujo de entrada para leer datos binarios.

**Métodos:**
- `read(uint8_t*, size_t)`: Lee hasta length bytes en el buffer, devuelve el número de bytes leídos o -1 si se alcanza el final del flujo.
- `skip(size_t)`: Salta n bytes en el flujo.
- `close()`: Cierra el flujo y libera recursos.

### OutputStream
Una clase base abstracta que representa un flujo de salida para escribir datos binarios.

**Métodos:**
- `write(const uint8_t*, size_t)`: Escribe length bytes desde el buffer.
- `flush()`: Vacía cualquier dato almacenado en búfer.
- `close()`: Cierra el flujo y libera recursos.

### Blob
Una clase base abstracta que representa un objeto binario grande (BLOB).

**Métodos:**
- `length()`: Devuelve la longitud del BLOB en bytes.
- `getBytes(size_t, size_t)`: Recupera una porción del BLOB como un vector de bytes.
- `getBinaryStream()`: Obtiene un flujo para leer desde el BLOB.
- `setBinaryStream(size_t)`: Obtiene un flujo para escribir en el BLOB a partir de la posición indicada.
- `setBytes(size_t, const std::vector<uint8_t>&)`: Escribe bytes en el BLOB a partir de la posición indicada.
- `setBytes(size_t, const uint8_t*, size_t)`: Escribe bytes en el BLOB a partir de la posición indicada.
- `truncate(size_t)`: Trunca el BLOB a la longitud especificada.
- `free()`: Libera los recursos asociados con el BLOB.

### ResultSet
Una clase base abstracta que representa un conjunto de resultados de una consulta SQL.

**Métodos:**
- `next()`: Avanza a la siguiente fila, devuelve true si tiene éxito.
- `isBeforeFirst()`: Devuelve true si el cursor está antes de la primera fila.
- `isAfterLast()`: Devuelve true si el cursor está después de la última fila.
- `getRow()`: Devuelve el número de fila actual.
- `getInt(int/string)`: Recupera el valor de la columna designada como un entero.
- `getLong(int/string)`: Recupera el valor de la columna designada como un int64_t (portable en todas las plataformas).
- `getDouble(int/string)`: Recupera el valor de la columna designada como un double.
- `getString(int/string)`: Recupera el valor de la columna designada como una cadena.
- `getBoolean(int/string)`: Recupera el valor de la columna designada como un booleano.
- `isNull(int/string)`: Devuelve true si el valor de la columna designada es nulo.
- `getColumnNames()`: Devuelve un vector con todos los nombres de columnas.
- `getColumnCount()`: Devuelve el número de columnas en el conjunto de resultados.
- `getBlob(int/string)`: Recupera el valor de la columna designada como un objeto Blob.
- `getBinaryStream(int/string)`: Recupera el valor de la columna designada como un flujo de entrada binario.
- `getBytes(int/string)`: Recupera el valor de la columna designada como un vector de bytes.

### PreparedStatement
Una clase base abstracta que representa una declaración SQL precompilada.

**Métodos:**
- `setInt(int, int)`: Establece el parámetro designado al valor entero dado.
- `setLong(int, int64_t)`: Establece el parámetro designado al valor int64_t dado (portable en todas las plataformas).
- `setDouble(int, double)`: Establece el parámetro designado al valor double dado.
- `setString(int, string)`: Establece el parámetro designado al valor de cadena dado.
- `setBoolean(int, bool)`: Establece el parámetro designado al valor booleano dado.
- `setNull(int, Types)`: Establece el parámetro designado a SQL NULL.
- `setBlob(int, Blob)`: Establece el parámetro designado al objeto Blob dado.
- `setBinaryStream(int, InputStream)`: Establece el parámetro designado al flujo de entrada binario dado.
- `setBinaryStream(int, InputStream, size_t)`: Establece el parámetro designado al flujo de entrada binario dado con la longitud especificada.
- `setBytes(int, const std::vector<uint8_t>&)`: Establece el parámetro designado al vector de bytes dado.
- `setBytes(int, const uint8_t*, size_t)`: Establece el parámetro designado a los bytes dados con la longitud especificada.
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
- `setTransactionIsolation(TransactionIsolationLevel)`: Establece el nivel de aislamiento de transacción.
- `getTransactionIsolation()`: Devuelve el nivel de aislamiento de transacción actual.

### Driver
Una clase base abstracta que representa un controlador de base de datos.

**Métodos:**
- `connect(string, string, string, map<string, string>)`: Establece una conexión a la base de datos con opciones de conexión opcionales.
- `connectRelational(string, string, string, map<string, string>)`: Establece una conexión relacional a la base de datos con opciones de conexión opcionales.
- `acceptURI(string)`: Devuelve true si el controlador puede conectarse a la URL dada.

### DriverManager
Una clase gestora para registrar y recuperar instancias de controladores.

**Métodos:**
- `registerDriver(string, Driver)`: Registra un controlador con el nombre dado.
- `getDBConnection(string, string, string, map<string, string>)`: Obtiene una conexión a la base de datos especificada por la URL con opciones de conexión opcionales.
- `getDBConnection(DatabaseConfig)`: Obtiene una conexión utilizando un objeto de configuración de base de datos.

---

## Soporte para BLOB
*Componentes definidos en blob.hpp*

CPP_DBC proporciona soporte completo para objetos binarios grandes (BLOB) en todas las bases de datos soportadas. La implementación sigue un diseño similar a JDBC, con clases base abstractas y implementaciones específicas para cada motor de base de datos.

### Seguridad de Memoria en BLOB

Todas las implementaciones de BLOB utilizan punteros inteligentes (`std::weak_ptr`) para referencias seguras a la conexión:

- **FirebirdBlob**: Usa `weak_ptr<FirebirdConnection>` con el método auxiliar `getConnection()`
- **MySQLBlob**: Usa `weak_ptr<MySQLDBConnection>` para acceso seguro a la conexión
- **PostgreSQLBlob**: Usa `weak_ptr<PGconn>` con el método auxiliar `getPGConnection(std::nothrow)`
- **SQLiteBlob**: Usa `weak_ptr<sqlite3>` con el método auxiliar `getSQLiteConnection()`

Todas las clases BLOB tienen un método `isConnectionValid()` para verificar si la conexión sigue siendo válida. Las operaciones lanzan `DBException` si la conexión ha sido cerrada, previniendo errores de uso después de liberación (use-after-free). La clase base `BlobStream` declara `isConnectionValid()` como virtual puro, y `MemoryBlob` (BLOBs en memoria sin conexión a base de datos) siempre retorna `true`.

### API de Versión e Información

Todos los drivers exponen una API unificada de introspección de versión/información a través de las clases base:

**`DBDriver` (clase base):**
- `getDriverVersion()`: Retorna la versión de la biblioteca cliente C/C++ subyacente (ej., `mysql_get_client_info()`, `PQlibVersion()`, `sqlite3_libversion()`)

**`DBConnection` (clase base):**
- `getServerVersion()` / `getServerVersion(std::nothrow_t)`: Retorna la versión del servidor de base de datos
- `getServerInfo()` / `getServerInfo(std::nothrow_t)`: Retorna `std::map<std::string, std::string>` con al menos `"ServerVersion"` más claves de metadatos específicas del driver

**Claves de metadatos por driver:**

| Driver | Claves en `getServerInfo()` |
|--------|--------------------------|
| MySQL | ServerVersion, ServerVersionNumeric, HostInfo, ProtocolVersion, CharacterSet, ThreadId, ServerStatus |
| PostgreSQL | ServerVersion, ServerVersionNumeric, ProtocolVersion, ServerEncoding, ClientEncoding, TimeZone, IntegerDatetimes, StandardConformingStrings |
| SQLite | ServerVersion, ServerVersionNumeric, SourceId, ThreadSafe |
| Firebird | ServerVersion, ODSMajorVersion, ODSMinorVersion, PageSize, SQLDialect |
| MongoDB | ServerVersion, GitVersion, SysInfo, Allocator, JavascriptEngine, Bits, MaxBsonObjectSize |
| Redis | ServerVersion + todos los campos de la respuesta INFO |
| ScyllaDB | ServerVersion, ClusterName, DataCenter, Rack, Partitioner, CQLVersion |

**Nota:** MongoDB también proporciona `getServerInfoAsDocument()` que retorna la respuesta completa de buildInfo como un objeto documento enriquecido, separado del `getServerInfo()` basado en `std::map`.

### Clases Base

#### MemoryInputStream
Implementación base de InputStream que lee desde un buffer de memoria.

**Métodos:**
- `MemoryInputStream(const std::vector<uint8_t>&)`: Constructor que toma un vector de datos.
- `read(uint8_t*, size_t)`: Lee datos del buffer de memoria.
- `skip(size_t)`: Salta bytes en el flujo.
- `close()`: Cierra el flujo.

#### MemoryOutputStream
Implementación base de OutputStream que escribe en un buffer de memoria.

**Métodos:**
- `MemoryOutputStream(std::vector<uint8_t>&, size_t)`: Constructor que toma un vector de datos y una posición inicial.
- `write(const uint8_t*, size_t)`: Escribe datos en el buffer de memoria.
- `flush()`: Vacía cualquier dato almacenado en búfer.
- `close()`: Cierra el flujo.

#### FileInputStream
Implementación de InputStream que lee desde un archivo.

**Métodos:**
- `FileInputStream(const std::string&)`: Constructor que toma un nombre de archivo.
- `read(uint8_t*, size_t)`: Lee datos del archivo.
- `skip(size_t)`: Salta bytes en el flujo.
- `close()`: Cierra el archivo.

#### FileOutputStream
Implementación de OutputStream que escribe en un archivo.

**Métodos:**
- `FileOutputStream(const std::string&, bool)`: Constructor que toma un nombre de archivo y una bandera de anexar.
- `write(const uint8_t*, size_t)`: Escribe datos en el archivo.
- `flush()`: Vacía cualquier dato almacenado en búfer.
- `close()`: Cierra el archivo.

#### MemoryBlob
Implementación base de Blob que almacena datos en memoria.

**Métodos:**
- `MemoryBlob()`: Constructor predeterminado.
- `MemoryBlob(const std::vector<uint8_t>&)`: Constructor que toma un vector de datos inicial.
- `length()`: Devuelve la longitud del BLOB.
- `getBytes(size_t, size_t)`: Recupera una porción del BLOB como un vector de bytes.
- `getBinaryStream()`: Obtiene un flujo para leer desde el BLOB.
- `setBinaryStream(size_t)`: Obtiene un flujo para escribir en el BLOB a partir de la posición indicada.
- `setBytes(size_t, const std::vector<uint8_t>&)`: Escribe bytes en el BLOB a partir de la posición indicada.
- `setBytes(size_t, const uint8_t*, size_t)`: Escribe bytes en el BLOB a partir de la posición indicada.
- `truncate(size_t)`: Trunca el BLOB a la longitud especificada.
- `free()`: Libera los recursos asociados con el BLOB.

### Uso de BLOB

Para trabajar con BLOB en CPP_DBC, se pueden utilizar los siguientes patrones:

1. **Lectura de BLOB desde la base de datos:**
   ```cpp
   auto rs = conn->executeQuery("SELECT data FROM test_blobs WHERE id = 1");
   if (rs->next()) {
       // Obtener como vector de bytes
       auto blobData = rs->getBytes("data");
       
       // O como objeto Blob
       auto blob = rs->getBlob("data");
       
       // O como flujo de entrada
       auto inputStream = rs->getBinaryStream("data");
   }
   ```

2. **Escritura de BLOB en la base de datos:**
   ```cpp
   auto stmt = conn->prepareStatement("INSERT INTO test_blobs (id, name, data) VALUES (?, ?, ?)");
   stmt->setInt(1, 1);
   stmt->setString(2, "Test BLOB");
   
   // Usando vector de bytes
   std::vector<uint8_t> data = {...};
   stmt->setBytes(3, data);
   
   // O usando un objeto Blob
   auto blob = std::make_shared<cpp_dbc::MemoryBlob>(data);
   stmt->setBlob(3, blob);
   
   // O usando un flujo de entrada
   auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(data);
   stmt->setBinaryStream(3, inputStream, data.size());
   
   stmt->executeUpdate();
   ```

---

## Arquitectura — Dual-API Nothrow-First (Todos los Controladores)

Los 7 controladores de base de datos comparten una arquitectura consistente:

- **Guardas `#ifdef __cpp_exceptions`**: Los métodos con excepciones se compilan condicionalmente; los métodos nothrow siempre se compilan, habilitando builds `-fno-exceptions`
- **Nothrow-first**: La lógica real reside en métodos `noexcept` que retornan `expected<T, DBException>`. Los métodos con excepciones son wrappers delgados que llaman la versión nothrow y re-lanzan en caso de error
- **Patrón de fábrica estática**: Las clases de conexión proveen fábricas `create(std::nothrow_t, ...)` con constructores privados nothrow. Diferimiento de errores vía miembros `m_initFailed`/`m_initError`
- **Double-checked locking**: Todos los controladores usan `std::atomic<bool>` + `std::mutex` para inicialización única de la biblioteca C (no `std::once_flag` — permite re-inicialización después de `cleanup()`, evita `std::system_error` bajo `-fno-exceptions`)
- **Eliminación de try/catch muerto**: Los métodos nothrow que solo llaman otros métodos nothrow no tienen bloques try/catch (según convenciones del proyecto)

---

## Implementación MySQL
*Componentes definidos en drivers/relational/driver_mysql.hpp (agregador), drivers/relational/mysql/*.hpp (clases individuales) y src/drivers/relational/mysql/*.cpp*

### MySQLInputStream
Implementación de InputStream para MySQL.

**Métodos:**
Los mismos que InputStream, más:
- `MySQLInputStream(const char*, size_t)`: Constructor que toma un buffer y su longitud.

### MySQLBlob
Implementación de Blob para MySQL.

**Métodos:**
Los mismos que Blob, más:
- `MySQLBlob(MYSQL*)`: Constructor para crear un nuevo BLOB.
- `MySQLBlob(MYSQL*, const std::string&, const std::string&, const std::string&)`: Constructor para cargar un BLOB existente de una tabla, columna y cláusula WHERE específicas.
- `MySQLBlob(MYSQL*, const std::vector<uint8_t>&)`: Constructor para crear un BLOB a partir de datos existentes.
- `ensureLoaded()`: Asegura que los datos del BLOB estén cargados desde la base de datos.
- `save()`: Guarda los datos del BLOB en la base de datos.

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
- `MySQLConnection(string, int, string, string, string, map<string, string>)`: Constructor que toma host, puerto, base de datos, usuario, contraseña y opciones de conexión opcionales.
- `setTransactionIsolation(TransactionIsolationLevel)`: Establece el nivel de aislamiento de transacción para MySQL (predeterminado: REPEATABLE READ).
- `getTransactionIsolation()`: Devuelve el nivel de aislamiento de transacción actual.

### MySQLDBDriver
Implementación de Driver para MySQL.

**Métodos:**
Los mismos que Driver, más:
- `MySQLDBDriver()`: Constructor que inicializa la biblioteca MySQL.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Implementación PostgreSQL
*Componentes definidos en drivers/relational/driver_postgresql.hpp (agregador), drivers/relational/postgresql/*.hpp (clases individuales) y src/drivers/relational/postgresql/*.cpp*

### PostgreSQLInputStream
Implementación de InputStream para PostgreSQL.

**Métodos:**
Los mismos que InputStream, más:
- `PostgreSQLInputStream(const char*, size_t)`: Constructor que toma un buffer y su longitud.

### PostgreSQLBlob
Implementación de Blob para PostgreSQL.

**Métodos:**
Los mismos que Blob, más:
- `PostgreSQLBlob(PGconn*)`: Constructor para crear un nuevo BLOB.
- `PostgreSQLBlob(PGconn*, Oid)`: Constructor para cargar un BLOB existente por su OID.
- `PostgreSQLBlob(PGconn*, const std::vector<uint8_t>&)`: Constructor para crear un BLOB a partir de datos existentes.
- `ensureLoaded()`: Asegura que los datos del BLOB estén cargados desde la base de datos.
- `save()`: Guarda los datos del BLOB en la base de datos y devuelve el OID.
- `getOid()`: Devuelve el OID del objeto grande.

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
- `PostgreSQLConnection(string, int, string, string, string, map<string, string>)`: Constructor que toma host, puerto, base de datos, usuario, contraseña y opciones de conexión opcionales.
- `generateStatementName()`: Genera un nombre único para declaraciones preparadas.
- `setTransactionIsolation(TransactionIsolationLevel)`: Establece el nivel de aislamiento de transacción para PostgreSQL (predeterminado: READ COMMITTED).
- `getTransactionIsolation()`: Devuelve el nivel de aislamiento de transacción actual.

### PostgreSQLDBDriver
Implementación de Driver para PostgreSQL.

**Métodos:**
Los mismos que Driver, más:
- `PostgreSQLDBDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Implementación SQLite
*Componentes definidos en drivers/relational/driver_sqlite.hpp (agregador), drivers/relational/sqlite/*.hpp (clases individuales) y src/drivers/relational/sqlite/*.cpp*

### SQLiteInputStream
Implementación de InputStream para SQLite.

**Métodos:**
Los mismos que InputStream, más:
- `SQLiteInputStream(const void*, size_t)`: Constructor que toma un buffer y su longitud.

### SQLiteBlob
Implementación de Blob para SQLite.

**Métodos:**
Los mismos que Blob, más:
- `SQLiteBlob(sqlite3*)`: Constructor para crear un nuevo BLOB.
- `SQLiteBlob(sqlite3*, const std::string&, const std::string&, const std::string&)`: Constructor para cargar un BLOB existente de una tabla, columna y rowId específicos.
- `SQLiteBlob(sqlite3*, const std::vector<uint8_t>&)`: Constructor para crear un BLOB a partir de datos existentes.
- `ensureLoaded()`: Asegura que los datos del BLOB estén cargados desde la base de datos.
- `save()`: Guarda los datos del BLOB en la base de datos.

### SQLiteResultSet
Implementación de ResultSet para SQLite.

**Métodos:**
Los mismos que ResultSet, más:
- `SQLiteResultSet(sqlite3_stmt*, bool)`: Constructor que toma una sentencia SQLite y una bandera de propiedad.

### SQLitePreparedStatement
Implementación de PreparedStatement para SQLite.

**Métodos:**
Los mismos que PreparedStatement, más:
- `SQLitePreparedStatement(sqlite3*, string)`: Constructor que toma una conexión SQLite y una declaración SQL.

### SQLiteConnection
Implementación de Connection para SQLite.

**Métodos:**
Los mismos que Connection, más:
- `SQLiteConnection(string, map<string, string>)`: Constructor que toma una ruta de base de datos y opciones de conexión opcionales.
- `setTransactionIsolation(TransactionIsolationLevel)`: Establece el nivel de aislamiento de transacción para SQLite (solo se admite SERIALIZABLE).
- `getTransactionIsolation()`: Devuelve el nivel de aislamiento de transacción actual.
- `registerStatement(std::weak_ptr<SQLitePreparedStatement>)`: Registra una declaración con la conexión para una limpieza adecuada.
- `unregisterStatement(std::weak_ptr<SQLitePreparedStatement>)`: Anula el registro de una declaración de la conexión.

**Herencia:**
- Hereda de `Connection` y `std::enable_shared_from_this<SQLiteConnection>` para una gestión adecuada de recursos.

**Uso de Punteros Inteligentes:**
- Usa `shared_ptr<sqlite3>` con deleter personalizado (`SQLiteDbDeleter`) para el handle de conexión
- Los PreparedStatements usan `weak_ptr<sqlite3>` para detectar de forma segura cuando la conexión está cerrada
- Las declaraciones activas se rastrean mediante `set<weak_ptr<SQLitePreparedStatement>>` para evitar impedir la destrucción

### SQLiteDBDriver
Implementación de Driver para SQLite.

**Métodos:**
Los mismos que Driver, más:
- `SQLiteDBDriver()`: Constructor.
- `parseURL(string, string&)`: Analiza una URL de conexión.

---

## Implementación Firebird
*Componentes definidos en drivers/relational/driver_firebird.hpp (agregador), drivers/relational/firebird/*.hpp (clases individuales) y src/drivers/relational/firebird/*.cpp*

### FirebirdInputStream
Implementación de InputStream para Firebird.

**Métodos:**
Los mismos que InputStream, más:
- `FirebirdInputStream(const char*, size_t)`: Constructor que toma un buffer y su longitud.

### FirebirdBlob
Implementación de Blob para Firebird.

**Métodos:**
Los mismos que Blob, más:
- `FirebirdBlob(std::shared_ptr<isc_db_handle>)`: Constructor para crear un nuevo BLOB.
- `FirebirdBlob(std::shared_ptr<isc_db_handle>, ISC_QUAD)`: Constructor para cargar un BLOB existente por su ID.
- `FirebirdBlob(std::shared_ptr<isc_db_handle>, const std::vector<uint8_t>&)`: Constructor para crear un BLOB a partir de datos existentes.
- `ensureLoaded()`: Asegura que los datos del BLOB estén cargados desde la base de datos.
- `save()`: Guarda los datos del BLOB en la base de datos y devuelve el ID del BLOB.
- `getBlobId()`: Devuelve el ID del BLOB.

### FirebirdResultSet
Implementación de ResultSet para Firebird.

**Métodos:**
Los mismos que ResultSet, más:
- `FirebirdResultSet(isc_stmt_handle, XSQLDA*, std::weak_ptr<isc_db_handle>)`: Constructor que toma un handle de sentencia, descriptor de salida y referencia débil a la conexión.

### FirebirdPreparedStatement
Implementación de PreparedStatement para Firebird.

**Métodos:**
Los mismos que PreparedStatement, más:
- `FirebirdPreparedStatement(std::weak_ptr<isc_db_handle>, string)`: Constructor que toma una referencia débil a la conexión Firebird y una declaración SQL.

### FirebirdConnection
Implementación de Connection para Firebird.

**Métodos:**
Los mismos que Connection, más:
- `FirebirdConnection(string, int, string, string, string, map<string, string>)`: Constructor que toma host, puerto, base de datos, usuario, contraseña y opciones de conexión opcionales.
- `setTransactionIsolation(TransactionIsolationLevel)`: Establece el nivel de aislamiento de transacción para Firebird (predeterminado: READ COMMITTED).
- `getTransactionIsolation()`: Devuelve el nivel de aislamiento de transacción actual.
- `registerStatement(std::weak_ptr<FirebirdPreparedStatement>)`: Registra una declaración con la conexión para una limpieza adecuada.
- `unregisterStatement(std::weak_ptr<FirebirdPreparedStatement>)`: Anula el registro de una declaración de la conexión.

**Herencia:**
- Hereda de `Connection` y `std::enable_shared_from_this<FirebirdConnection>` para una gestión adecuada de recursos.

**Uso de Punteros Inteligentes:**
- Usa `shared_ptr<isc_db_handle>` con deleter personalizado (`FirebirdDbDeleter`) para el handle de conexión
- Los PreparedStatements usan `weak_ptr<isc_db_handle>` para detectar de forma segura cuando la conexión está cerrada
- Las declaraciones activas se rastrean mediante `set<weak_ptr<FirebirdPreparedStatement>>` para evitar impedir la destrucción

### FirebirdDBDriver
Implementación de Driver para Firebird.

**Métodos:**
Los mismos que Driver, más:
- `FirebirdDBDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.
- `command(map<string, any>)`: Ejecuta comandos específicos del controlador (ej., "create_database").
- `createDatabase(string, string, string, map<string, string>)`: Crea una nueva base de datos Firebird con tamaño de página y charset opcionales.

**Creación de Base de Datos:**
El controlador Firebird soporta la creación de nuevas bases de datos programáticamente:

```cpp
auto driver = std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>();

// Usando el método command
std::map<std::string, std::any> params = {
    {"command", std::string("create_database")},
    {"url", std::string("cpp_dbc:firebird://localhost:3050/ruta/a/nueva.fdb")},
    {"user", std::string("SYSDBA")},
    {"password", std::string("masterkey")},
    {"page_size", std::string("4096")},  // opcional, predeterminado: 4096
    {"charset", std::string("UTF8")}     // opcional, predeterminado: UTF8
};
driver->command(params);

// O usando el método createDatabase directamente
driver->createDatabase(
    "cpp_dbc:firebird://localhost:3050/ruta/a/nueva.fdb",
    "SYSDBA",
    "masterkey",
    {{"page_size", "8192"}, {"charset", "UTF8"}}
);
```

**Formato de URL de Conexión:**
```
cpp_dbc:firebird://host:port/ruta/a/base_de_datos.fdb
```

**Ejemplo de Uso:**
```cpp
#if USE_FIREBIRD
#include <cpp_dbc/drivers/driver_firebird.hpp>

// Registrar el controlador Firebird
cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::FirebirdDBDriver>());

// Conectar a la base de datos
auto conn = cpp_dbc::DriverManager::getDBConnection(
    "cpp_dbc:firebird://localhost:3050/ruta/a/base_de_datos.fdb",
    "SYSDBA",
    "masterkey"
);

// Ejecutar consultas
auto rs = conn->executeQuery("SELECT * FROM mi_tabla");
while (rs->next()) {
    std::cout << rs->getString("columna") << std::endl;
}

conn->close();
#endif
```

---

## Implementación MongoDB
*Componentes definidos en drivers/document/driver_mongodb.hpp y src/drivers/document/driver_mongodb.cpp*

MongoDB es una base de datos de documentos que almacena datos en documentos flexibles similares a JSON. La biblioteca CPP_DBC proporciona una implementación completa del controlador MongoDB con soporte para operaciones CRUD, gestión de colecciones e iteración basada en cursores. El controlador MongoDB sigue el patrón dual-API nothrow-first: toda la lógica real reside en métodos `noexcept` que retornan `expected<T, DBException>`, y la API con excepciones es una capa delgada de wrappers. Todas las interfaces abstractas protegen los métodos con excepciones usando `#ifdef __cpp_exceptions` para compatibilidad con `-fno-exceptions`.

### DocumentDBData
Una clase base abstracta que representa datos de documentos.

**Métodos:**
- `toJson()`: Devuelve el documento como una cadena JSON.
- `getString(string)`: Obtiene un valor de cadena por clave.
- `getInt(string)`: Obtiene un valor entero por clave.
- `getDouble(string)`: Obtiene un valor double por clave.
- `getBool(string)`: Obtiene un valor booleano por clave.
- `hasField(string)`: Devuelve true si el documento tiene el campo especificado.

### DocumentDBCursor
Una clase base abstracta que representa un cursor para iterar sobre documentos.

**Métodos:**
- `next()`: Avanza al siguiente documento, devuelve true si tiene éxito.
- `getData()`: Devuelve los datos del documento actual.
- `hasNext()`: Devuelve true si hay más documentos.

### DocumentDBCollection
Una clase base abstracta que representa una colección de documentos.

**Métodos:**
- `insertOne(string)`: Inserta un solo documento (cadena JSON).
- `insertMany(vector<string>)`: Inserta múltiples documentos.
- `find(string)`: Encuentra documentos que coinciden con el filtro, devuelve un cursor.
- `findOne(string)`: Encuentra un solo documento que coincide con el filtro.
- `updateOne(string, string)`: Actualiza un solo documento que coincide con el filtro.
- `updateMany(string, string)`: Actualiza múltiples documentos que coinciden con el filtro.
- `deleteOne(string)`: Elimina un solo documento que coincide con el filtro.
- `deleteMany(string)`: Elimina múltiples documentos que coinciden con el filtro.
- `countDocuments(string)`: Cuenta documentos que coinciden con el filtro.
- `createIndex(string)`: Crea un índice en la colección.
- `dropIndex(string)`: Elimina un índice de la colección.

### DocumentDBConnection
Una clase base abstracta que representa una conexión a una base de datos de documentos.

**Métodos:**
- `close()`: Cierra la conexión.
- `isClosed()`: Devuelve true si la conexión está cerrada.
- `getCollection(string)`: Obtiene una colección por nombre.
- `createCollection(string)`: Crea una nueva colección.
- `dropCollection(string)`: Elimina una colección.
- `listCollections()`: Lista todas las colecciones en la base de datos.

### DocumentDBDriver
Una clase base abstracta que representa un controlador de base de datos de documentos.

**Métodos:**
- `connectDocument(string, string, string, map<string, string>)`: Establece una conexión a la base de datos de documentos.
- `acceptURI(string)`: Devuelve true si el controlador puede conectarse a la URL dada.

### MongoDBDocument (anteriormente MongoDBData)
Implementación de DocumentDBData para MongoDB. Usa patrón de fábrica estática para construcción.

**Construcción:**
- `MongoDBDocument::create(std::nothrow_t)`: Crea un documento vacío (fábrica estática, noexcept).
- `MongoDBDocument::create(std::nothrow_t, bson_t*)`: Crea desde un documento BSON (fábrica estática, noexcept).

**Métodos:**
Los mismos que DocumentDBData, más:
- `getBson()`: Devuelve el documento BSON subyacente.
- `getBsonMutable()`: Devuelve una referencia mutable al documento BSON.
- Caché de ID: `getId()` almacena en caché el valor `_id` internamente para evitar recorridos BSON repetidos.

### MongoDBCursor
Implementación de DocumentDBCursor para MongoDB.

**Construcción:**
- `MongoDBCursor(std::nothrow_t, ...)`: Constructor nothrow.

**Métodos:**
Los mismos que DocumentDBCursor. Los métodos de encadenamiento (`skip`, `limit`, `sort`) retornan `std::reference_wrapper<DocumentDBCursor>` en la API nothrow.

### MongoDBCollection
Implementación de DocumentDBCollection para MongoDB.

**Métodos:**
Los mismos que DocumentDBCollection, más:
- Helper interno `getCollectionHandle(std::nothrow_t)` que combina obtención de cliente + colección.

### MongoDBConnection
Implementación de DocumentDBConnection para MongoDB. Usa patrón de fábrica estática.

**Construcción:**
- `MongoDBConnection::create(std::nothrow_t, url, user, password, options)`: Fábrica estática (noexcept), retorna `expected<shared_ptr<MongoDBConnection>, DBException>`.
- Constructor privado nothrow + helper `initialize(std::nothrow_t)` para inicialización falible.

**Métodos:**
Los mismos que DocumentDBConnection, más:
- `getURI()`: Devuelve la URI de conexión.

**Uso de Punteros Inteligentes:**
- Usa punteros inteligentes para una gestión adecuada de recursos
- Operaciones thread-safe (compiladas condicionalmente con `DB_DRIVER_THREAD_SAFE`)

### MongoDBDriver
Implementación de DocumentDBDriver para MongoDB.

**Construcción:**
- `MongoDBDriver()`: Constructor que usa double-checked locking (`std::atomic<bool>` + `std::mutex`) para inicialización única de la biblioteca C. Compatible con `-fno-exceptions` (reemplaza `std::call_once`).

**Métodos:**
Los mismos que DocumentDBDriver, más:
- `acceptURI(string)`: Devuelve true solo para URLs `cpp_dbc:mongodb://`.
- `cleanup()`: Reinicia el estado de inicialización, permitiendo re-inicialización.
- `isInitialized()`: Verifica si la biblioteca C está inicializada.

**Formato de URL de Conexión:**
```
cpp_dbc:mongodb://host:puerto/base_de_datos
cpp_dbc:mongodb://usuario:contraseña@host:puerto/base_de_datos?authSource=admin
```

**Ejemplo de Uso:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>

// Registrar el controlador MongoDB
cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());

// Conectar a MongoDB
auto conn = cpp_dbc::DriverManager::getDocumentDBConnection(
    "mongodb://localhost:27017/mydb", "usuario", "contraseña");

// Obtener una colección
auto collection = conn->getCollection("usuarios");

// Insertar un documento
collection->insertOne(R"({"nombre": "Juan", "edad": 30})");

// Buscar documentos
auto cursor = collection->find(R"({"edad": {"$gte": 25}})");
while (cursor->next()) {
    auto data = cursor->getData();
    std::cout << data->toJson() << std::endl;
}

// Actualizar un documento
collection->updateOne(
    R"({"nombre": "Juan"})",
    R"({"$set": {"edad": 31}})"
);

// Eliminar un documento
collection->deleteOne(R"({"nombre": "Juan"})");

// Cerrar la conexión
conn->close();
#endif
```

---

## Pool de Conexiones
*Componentes definidos en pool/connection_pool.hpp, pool/pooled_db_connection_base.hpp, y pool/\<familia\>/\<familia\>_db_connection_pool.hpp*

### Arquitectura

El sistema de pool tiene tres capas:

1. **`DBConnectionPoolBase`** (`pool/connection_pool.hpp` / `.cpp`) — toda la infraestructura compartida del pool: ciclo de vida de conexiones, hilo de mantenimiento (30s), entrega directa (`ConnectionRequest` + `m_waitQueue`), validación estilo HikariCP (umbral configurable `validationTimeoutMillis`, default 5000ms), protocolo de bloqueo por fases. Heredado por cada pool de familia.
2. **`PooledDBConnectionBase<Derived, ConnType, PoolType>`** (`pool/pooled_db_connection_base.hpp` / `.cpp`) — template CRTP con la lógica común de conexiones agrupadas: close/returnToPool (corrección de condición de carrera), limpieza en destructor, accesores de metadatos del pool. Proporciona métodos `*Impl` para métodos ambiguos de DBConnection (diamante) y métodos `*Throw` para delegadores con excepciones. Instanciaciones explícitas para las 4 familias.
3. **`<Familia>DBConnectionPool`** + **`<Familia>PooledDBConnection`** (`pool/<familia>/<familia>_db_connection_pool.hpp` / `.cpp`) — clases derivadas delgadas. La clase pool sobrescribe `createPooledDBConnection()` y agrega un getter tipado. La clase de conexión agrupada proporciona delegadores inline de una línea para resolver la herencia diamante e implementa solo los métodos específicos de la familia.

### DBConnectionPool
Clase abstracta que define la interfaz común para todos los pools de conexiones.

**Métodos:**
- `getDBConnection(std::nothrow_t)` / `getDBConnection()`: Obtiene una conexión desde el pool.
- `getActiveDBConnectionCount(std::nothrow_t)`: Devuelve el número de conexiones activas.
- `getIdleDBConnectionCount(std::nothrow_t)`: Devuelve el número de conexiones inactivas.
- `getTotalDBConnectionCount(std::nothrow_t)`: Devuelve el número total de conexiones.
- `close(std::nothrow_t)` / `close()`: Cierra el pool y todas las conexiones.
- `isRunning(std::nothrow_t)`: Devuelve si el pool está en funcionamiento.

### DBConnectionPoolConfig
Estructura de configuración para pools de conexiones.

**Propiedades:**
- `url`: La URL de la base de datos
- `username`: El nombre de usuario de la base de datos
- `password`: La contraseña de la base de datos
- `options`: Mapa de opciones de conexión específicas del tipo de base de datos
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

### DBConnectionPoolBase
Clase base unificada para todos los pools de conexiones.

**Métodos clave (protegidos/internos):**
- `acquireConnection(std::nothrow_t)`: Lógica central de préstamo — intenta cola inactiva, crea nueva si bajo máximo, o espera vía entrega directa.
- `returnConnection(std::nothrow_t, shared_ptr<DBConnectionPooled>)`: Devuelve conexión — maneja detección de huérfanos, entrega directa a esperadores, salida por cierre de pool.
- `decrementActiveCount(std::nothrow_t)`: Abandono seguro de conexión en destructor cuando el pool está muerto.
- `initializePool(std::nothrow_t)`: Crea conexiones iniciales; llamado por métodos factory derivados después de `make_shared`.
- `createPooledDBConnection(std::nothrow_t)`: Virtual puro — clases derivadas sobrescriben para crear envolturas de conexión agrupada específicas de familia.
- `maintenanceTask(std::nothrow_t)`: Hilo en segundo plano — evicción de inactivas + reposición de minIdle cada 30s.

### PooledDBConnectionBase (Template CRTP)
Envuelve una conexión física para proporcionar funcionalidad de agrupamiento. Todas las conexiones agrupadas específicas de familia heredan de este template.

**Parámetros de Template:** `<Derived, ConnType, PoolType>` (ej., `<RelationalPooledDBConnection, RelationalDBConnection, RelationalDBConnectionPool>`)

**Miembros protegidos:**
- `m_conn`: La conexión física subyacente (`shared_ptr<ConnType>`)
- `m_pool`: Referencia débil al pool (`weak_ptr<PoolType>`)
- `m_poolAlive`: Bandera compartida para verificar si el pool sigue vivo (`shared_ptr<atomic<bool>>`)
- `m_creationTime`, `m_lastUsedTimeNs` (atomic int64_t), `m_active` (atomic bool), `m_closed` (atomic bool)

**Métodos Impl para resolución de diamante:** `closeImpl`, `returnToPoolImpl`, `isClosedImpl`, `isPooledImpl`, `getURIImpl`, `resetImpl`, `pingImpl`, `prepareForPoolReturnImpl`, `prepareForBorrowImpl`

**Métodos de interfaz DBConnectionPooled:**
- `getCreationTime(std::nothrow_t)`: Devuelve el tiempo de creación.
- `getLastUsedTime(std::nothrow_t)`: Devuelve el último tiempo de uso.
- `setActive(std::nothrow_t, bool)`: Establece si la conexión está activa.
- `isActive(std::nothrow_t)`: Devuelve si la conexión está activa.
- `getUnderlyingConnection(std::nothrow_t)`: Devuelve la conexión física subyacente.
- `isPoolValid(std::nothrow_t)`: Devuelve si el pool sigue activo.
- `markPoolClosed(std::nothrow_t, bool)`: Marca el pool como cerrado desde el lado del pool.
- `isPoolClosed(std::nothrow_t)`: Devuelve si el pool ha sido cerrado.
- `updateLastUsedTime(std::nothrow_t)`: Actualiza la marca de tiempo de último uso.

**Seguridad de Memoria:**
- Usa `weak_ptr<PoolType>` para la referencia al pool
- Usa `shared_ptr<atomic<bool>>` (`m_poolAlive`) para rastrear el tiempo de vida del pool
- El método `close()` verifica `isPoolValid()` antes de devolver la conexión al pool
- Previene errores de uso después de liberación (use-after-free) cuando el pool es destruido mientras las conexiones están en uso
- El destructor nunca llama a `returnToPool()`/`shared_from_this()` (el refcount ya es 0); en su lugar, llama a `decrementActiveCount()` si la conexión estaba activa y cierra `m_conn` directamente

### Implementaciones específicas de Pool por Familia

Cada familia de base de datos tiene un pool y un envoltorio de conexión agrupada:

- **Relacional:** `RelationalDBConnectionPool` + `RelationalPooledDBConnection` — pools específicos: `MySQLConnectionPool`, `PostgreSQLConnectionPool`, `SQLiteConnectionPool`, `FirebirdConnectionPool`
- **Documento:** `DocumentDBConnectionPool` + `DocumentPooledDBConnection` — pool específico: `MongoDBConnectionPool`
- **Columnar:** `ColumnarDBConnectionPool` + `ColumnarPooledDBConnection` — pool específico: `ScyllaDBConnectionPool`
- **Clave-Valor:** `KVDBConnectionPool` + `KVPooledDBConnection` — pool específico: `RedisDBConnectionPool`

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
- `getTransactionDBConnection(string)`: Obtiene la conexión para una transacción.
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
- `getOptionsObj()`: Obtiene el objeto ConnectionOptions.
- `getOptions()`: Obtiene todas las opciones específicas de la base de datos como un mapa.
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
- `getDatabaseByName(string)`: Obtiene una referencia opcional a una configuración de base de datos por nombre.
- `getDatabases()`: Obtiene todas las configuraciones de bases de datos.
- `addConnectionPoolConfig(ConnectionPoolConfig)`: Agrega una configuración de pool de conexiones.
- `getConnectionPoolConfig(string)`: Obtiene una referencia opcional a una configuración de pool de conexiones por nombre.
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
- Bibliotecas de desarrollo de SQLite (para soporte SQLite, opcional)
- Bibliotecas de desarrollo de Firebird (para soporte Firebird SQL, opcional)
- Controlador C++ de MongoDB (libmongoc-dev, libbson-dev, libmongocxx-dev, libbsoncxx-dev para soporte MongoDB, opcional)
- Biblioteca yaml-cpp (para soporte de configuración YAML, opcional)
- Biblioteca libdw (parte de elfutils, para trazas de pila mejoradas, opcional)
- CMake 3.15 o posterior
- Conan para gestión de dependencias

### Advertencias del Compilador y Calidad del Código

La biblioteca cpp_dbc se compila con banderas de advertencia completas y comprobaciones en tiempo de compilación para garantizar una alta calidad del código:

```cmake
# Banderas de advertencia utilizadas por la biblioteca
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused
-Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef
-Wpointer-arith -Wcast-align
```

Estas banderas de advertencia ayudan a detectar posibles problemas:

- `-Wall -Wextra -Wpedantic`: Banderas de advertencia estándar
- `-Wconversion`: Advierte sobre conversiones implícitas que pueden cambiar un valor
- `-Wshadow`: Advierte cuando una declaración de variable oculta otra variable
- `-Wcast-qual`: Advierte sobre conversiones que eliminan calificadores de tipo
- `-Wformat=2`: Habilita comprobaciones adicionales de cadenas de formato
- `-Wunused`: Advierte sobre variables y funciones no utilizadas
- `-Werror=return-type`: Trata las declaraciones de retorno faltantes como errores
- `-Werror=switch`: Trata los problemas de declaraciones switch como errores
- `-Wdouble-promotion`: Advierte sobre promociones implícitas de float a double
- `-Wfloat-equal`: Advierte sobre comparaciones de igualdad de punto flotante
- `-Wundef`: Advierte sobre identificadores no definidos en expresiones del preprocesador
- `-Wpointer-arith`: Advierte sobre aritmética de punteros sospechosa
- `-Wcast-align`: Advierte sobre conversiones de punteros que aumentan los requisitos de alineación

La biblioteca también incluye un manejo especial para backward.hpp para silenciar las advertencias -Wundef:

```cmake
# Definir macros para backward.hpp para silenciar advertencias -Wundef
target_compile_definitions(cpp_dbc PUBLIC
    BACKWARD_HAS_UNWIND=0
    BACKWARD_HAS_LIBUNWIND=0
    BACKWARD_HAS_BACKTRACE=0
    BACKWARD_HAS_BFD=0
    BACKWARD_HAS_DWARF=0
    BACKWARD_HAS_BACKTRACE_SYMBOL=0
    BACKWARD_HAS_PDB_SYMBOL=0
)
```

Las mejoras de calidad del código incluyen:
- Uso del prefijo m_ para variables miembro para evitar advertencias -Wshadow
- Adición de static_cast<> para conversiones numéricas para evitar advertencias -Wconversion
- Cambio de tipos de retorno int a uint64_t para métodos executeUpdate()
- Mejora del manejo de excepciones para evitar el sombreado de variables
- Uso de `int64_t` en lugar de `long` para portabilidad multiplataforma (Windows/Linux/macOS)
- Funciones de tiempo multiplataforma (`localtime_s` en Windows, `localtime_r` en Unix)
- Atributo `[[nodiscard]]` para métodos que retornan `expected<T, DBException>`
- Mejoras de seguridad en operaciones BLOB de SQLite con `validateIdentifier()`
- Manejo seguro de excepciones en destructores con `std::scoped_lock`

### Compilación con Scripts

La biblioteca proporciona scripts de compilación para simplificar el proceso:

```bash
# Compilar con soporte para MySQL solamente (por defecto)
./build.sh

# Compilar con soporte para MySQL y PostgreSQL
./build.sh --mysql --postgres

# Compilar con soporte para PostgreSQL solamente
./build.sh --mysql-off --postgres

# Compilar con soporte para SQLite
./build.sh --sqlite

# Compilar con soporte para Firebird
./build.sh --firebird

# Compilar con soporte para MongoDB
./build.sh --mongodb

# Compilar con soporte para MySQL, PostgreSQL, SQLite, Firebird y MongoDB
./build.sh --mysql --postgres --sqlite --firebird --mongodb

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

# Deshabilitar soporte libdw para trazas de pila
./build.sh --dw-off

# Deshabilitar operaciones de controlador de base de datos thread-safe (para rendimiento en aplicaciones de un solo hilo)
./build.sh --db-driver-thread-safe-off

# Compilar contenedor Docker
./build.dist.sh

# Compilar contenedor Docker con soporte PostgreSQL y YAML
./build.dist.sh --postgres --yaml

# Compilar contenedor Docker con soporte SQLite
./build.dist.sh --sqlite

# Compilar contenedor Docker con soporte MongoDB
./build.dist.sh --mongodb

# Compilar contenedor Docker con todos los controladores de bases de datos
./build.dist.sh --postgres --sqlite --firebird --mongodb --yaml

# Compilar contenedor Docker sin soporte libdw
./build.dist.sh --dw-off

# Compilar contenedor Docker sin operaciones de controlador thread-safe
./build.dist.sh --db-driver-thread-safe-off

# Compilar paquetes de distribución (.deb y .rpm)
./libs/cpp_dbc/build_dist_pkg.sh

# Compilar paquetes para múltiples distribuciones
./libs/cpp_dbc/build_dist_pkg.sh --distro=ubuntu:24.04+ubuntu:22.04+debian:12+debian:13+fedora:42+fedora:43

# Compilar paquetes con opciones específicas
./libs/cpp_dbc/build_dist_pkg.sh --build=yaml,mysql,postgres,sqlite,debug,dw,examples

# Compilar paquetes con versión específica
./libs/cpp_dbc/build_dist_pkg.sh --version=1.0.1
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

El script `build_dist_pkg.sh`:
1. Crea contenedores Docker basados en las distribuciones objetivo
2. Compila la biblioteca cpp_dbc con las opciones especificadas
3. Crea paquetes de distribución (.deb para Debian/Ubuntu, .rpm para Fedora)
4. Copia los paquetes al directorio build

### Script de Ayuda

El proyecto incluye un script de ayuda (`helper.sh`) que proporciona varias utilidades:

```bash
# Compilar el proyecto con soporte MySQL
./helper.sh --build

# Compilar con soporte PostgreSQL
./helper.sh --build --postgres

# Compilar con soporte SQLite
./helper.sh --build --sqlite

# Compilar con soporte YAML
./helper.sh --build --yaml

# Compilar sin soporte libdw
./helper.sh --build --dw-off

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

# Luego ejecutar los ejemplos de configuración
./libs/cpp_dbc/run_examples.sh --run='10_01*'
# O usando helper.sh
./helper.sh --run-example='10_01*'
# O ejecutar ejemplo específico directamente
./build/libs/cpp_dbc/examples/10_011_example_config
```

Esto cargará el archivo de configuración YAML de ejemplo y mostrará las configuraciones de las bases de datos. También puede listar todos los ejemplos disponibles con `./libs/cpp_dbc/run_examples.sh --list`.

### Compilación Manual con CMake

También puedes compilar la biblioteca manualmente con CMake:

```bash
# Crear directorio de compilación para la biblioteca
mkdir -p libs/cpp_dbc/build
cd libs/cpp_dbc/build

# Configurar con CMake (MySQL habilitado, PostgreSQL deshabilitado, SQLite deshabilitado, Firebird deshabilitado, YAML deshabilitado, libdw habilitado, thread-safe habilitado)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DDB_DRIVER_THREAD_SAFE=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar con soporte Firebird
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=OFF -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DDB_DRIVER_THREAD_SAFE=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar sin operaciones de controlador thread-safe (para rendimiento en aplicaciones de un solo hilo)
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DDB_DRIVER_THREAD_SAFE=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar con soporte YAML
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar con soporte SQLite
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configurar sin soporte libdw
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Compilar e instalar
cmake --build . --target install

# Volver al directorio raíz
cd ../../..

# Crear directorio de compilación para la aplicación principal
mkdir -p build
cd build

# Configurar con CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configurar con soporte YAML
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=ON -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configurar con soporte SQLite
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=ON -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

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
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
    $<$<BOOL:${USE_FIREBIRD}>:USE_FIREBIRD=1>
    $<$<NOT:$<BOOL:${USE_FIREBIRD}>>:USE_FIREBIRD=0>
    $<$<BOOL:${USE_MONGODB}>:USE_MONGODB=1>
    $<$<NOT:$<BOOL:${USE_MONGODB}>>:USE_MONGODB=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
    $<$<BOOL:${BACKWARD_HAS_DW}>:BACKWARD_HAS_DW=1>
    $<$<NOT:$<BOOL:${BACKWARD_HAS_DW}>>:BACKWARD_HAS_DW=0>
    $<$<BOOL:${DB_DRIVER_THREAD_SAFE}>:DB_DRIVER_THREAD_SAFE=1>
    $<$<NOT:$<BOOL:${DB_DRIVER_THREAD_SAFE}>>:DB_DRIVER_THREAD_SAFE=0>
)
```

En tu código C++:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
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
auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
if (dbConfigOpt) {
    // Usar la configuración para crear una conexión
    const auto& dbConfig = dbConfigOpt->get();
    std::string connStr = dbConfig.createConnectionString();
    auto conn = cpp_dbc::DriverManager::getDBConnection(
        connStr, dbConfig.getUsername(), dbConfig.getPassword()
    );
    
    // Usar la conexión...
    conn->close();
}
#endif
