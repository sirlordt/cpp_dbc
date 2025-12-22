 Documentación de la Biblioteca CPP_DBC

Este documento proporciona una guía completa de la biblioteca CPP_DBC, una biblioteca de Conectividad de Bases de Datos para C++ inspirada en JDBC, con soporte para bases de datos MySQL, PostgreSQL, SQLite y Firebird.

## Tabla de Contenidos
- [Componentes Principales](#componentes-principales)
- [Soporte para BLOB](#soporte-para-blob)
- [Implementación MySQL](#implementación-mysql)
- [Implementación PostgreSQL](#implementación-postgresql)
- [Implementación SQLite](#implementación-sqlite)
- [Implementación Firebird](#implementación-firebird)
- [Pool de Conexiones](#pool-de-conexiones)
- [Gestor de Transacciones](#gestor-de-transacciones)
- [Sistema de Configuración](#sistema-de-configuración)
- [Compilación e Instalación](#compilación-e-instalación)

---

## Componentes Principales
*Componentes definidos en cpp_dbc.hpp*

### DBException
Una clase de excepción personalizada para errores relacionados con la base de datos.

**Métodos:**
- `DBException(const std::string& message)`: Constructor que toma un mensaje de error.
- `DBException(const std::string& mark, const std::string& message, const std::vector<system_utils::StackFrame>& callStack)`: Constructor que toma una marca de error, mensaje y pila de llamadas.
- `what_s()`: Devuelve el mensaje de error como un std::string (más seguro que what()).
- `getMark()`: Devuelve la marca única de error.
- `getCallStack()`: Devuelve la pila de llamadas capturada cuando se creó la excepción.
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
- `getLong(int/string)`: Recupera el valor de la columna designada como un long.
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
- `setLong(int, long)`: Establece el parámetro designado al valor long dado.
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
- `acceptsURL(string)`: Devuelve true si el controlador puede conectarse a la URL dada.

### DriverManager
Una clase gestora para registrar y recuperar instancias de controladores.

**Métodos:**
- `registerDriver(string, Driver)`: Registra un controlador con el nombre dado.
- `getConnection(string, string, string, map<string, string>)`: Obtiene una conexión a la base de datos especificada por la URL con opciones de conexión opcionales.
- `getConnection(DatabaseConfig)`: Obtiene una conexión utilizando un objeto de configuración de base de datos.

---

## Soporte para BLOB
*Componentes definidos en blob.hpp*

CPP_DBC proporciona soporte completo para objetos binarios grandes (BLOB) en todas las bases de datos soportadas. La implementación sigue un diseño similar a JDBC, con clases base abstractas y implementaciones específicas para cada motor de base de datos.

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

## Implementación MySQL
*Componentes definidos en drivers/driver_mysql.hpp, drivers/driver_mysql.cpp y drivers/mysql_blob.hpp*

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

### MySQLDriver
Implementación de Driver para MySQL.

**Métodos:**
Los mismos que Driver, más:
- `MySQLDriver()`: Constructor que inicializa la biblioteca MySQL.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Implementación PostgreSQL
*Componentes definidos en drivers/driver_postgresql.hpp, drivers/driver_postgresql.cpp y drivers/postgresql_blob.hpp*

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

### PostgreSQLDriver
Implementación de Driver para PostgreSQL.

**Métodos:**
Los mismos que Driver, más:
- `PostgreSQLDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.

---

## Implementación SQLite
*Componentes definidos en drivers/driver_sqlite.hpp, drivers/driver_sqlite.cpp y drivers/sqlite_blob.hpp*

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

### SQLiteDriver
Implementación de Driver para SQLite.

**Métodos:**
Los mismos que Driver, más:
- `SQLiteDriver()`: Constructor.
- `parseURL(string, string&)`: Analiza una URL de conexión.

---

## Implementación Firebird
*Componentes definidos en drivers/driver_firebird.hpp, drivers/driver_firebird.cpp y drivers/firebird_blob.hpp*

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

### FirebirdDriver
Implementación de Driver para Firebird.

**Métodos:**
Los mismos que Driver, más:
- `FirebirdDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Analiza una URL de conexión.
- `command(map<string, any>)`: Ejecuta comandos específicos del controlador (ej., "create_database").
- `createDatabase(string, string, string, map<string, string>)`: Crea una nueva base de datos Firebird con tamaño de página y charset opcionales.

**Creación de Base de Datos:**
El controlador Firebird soporta la creación de nuevas bases de datos programáticamente:

```cpp
auto driver = std::make_shared<cpp_dbc::Firebird::FirebirdDriver>();

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
cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::FirebirdDriver>());

// Conectar a la base de datos
auto conn = cpp_dbc::DriverManager::getConnection(
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

## Pool de Conexiones
*Componentes definidos en connection_pool.hpp y connection_pool.cpp*

### SQLiteConnectionPool
Implementación de ConnectionPool para bases de datos SQLite.

**Métodos:**
- `SQLiteConnectionPool(string, string, string)`: Constructor que toma una URL, nombre de usuario y contraseña.
- `SQLiteConnectionPool(ConnectionPoolConfig)`: Constructor que toma una configuración de pool.

### ConnectionPoolConfig
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

### ConnectionPool
Gestiona un pool de conexiones de base de datos.

**Métodos:**
- `ConnectionPool(string, string, string, map<string, string>, int, int, int, int, int, int, int, bool, bool, string)`: Constructor que toma parámetros de configuración individuales.
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
- `setTransactionIsolation(TransactionIsolationLevel)`: Delega a la conexión subyacente.
- `getTransactionIsolation()`: Delega a la conexión subyacente.

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

# Compilar con soporte para MySQL, PostgreSQL, SQLite y Firebird
./build.sh --mysql --postgres --sqlite --firebird

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

# Compilar contenedor Docker con todos los controladores de bases de datos
./build.dist.sh --postgres --sqlite --yaml

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
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#if USE_FIREBIRD
#include <cpp_dbc/drivers/driver_firebird.hpp>
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
    auto conn = cpp_dbc::DriverManager::getConnection(
        connStr, dbConfig.getUsername(), dbConfig.getPassword()
    );
    
    // Usar la conexión...
    conn->close();
}
#endif
